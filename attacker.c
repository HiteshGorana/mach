#include "attacker.h"
#include "stats.h"
#include "storage.h"
#include "ui.h"
#include <unistd.h>

static void *worker_thread(void *arg) {
  WorkerContext *ctx = (WorkerContext *)arg;
  Options *opts = ctx->opts;

  if (opts->ramp_up.tv_sec > 0 || opts->ramp_up.tv_nsec > 0) {
    double total_ramp =
        opts->ramp_up.tv_sec * 1000.0 + opts->ramp_up.tv_nsec / 1000000.0;
    double delay = (ctx->worker_id * total_ramp) / opts->concurrency;
    usleep(delay * 1000);
  }

  Connection *conn = NULL;

  // Decide how many requests to do
  int total_to_do = -1; // -1 means unlimited
  if (opts->duration_s == 0) {
    int requests_per_worker = opts->requests / opts->concurrency;
    int extra = (ctx->worker_id < (opts->requests % opts->concurrency)) ? 1 : 0;
    total_to_do = requests_per_worker + extra;
  }

  struct timespec start, now;
  clock_gettime(CLOCK_MONOTONIC, &start);

  for (int i = 0; (total_to_do == -1 || i < total_to_do) && !*(ctx->stop);
       i++) {
    // Check duration if set
    if (opts->duration_s > 0) {
      clock_gettime(CLOCK_MONOTONIC, &now);
      double elapsed = (now.tv_sec - start.tv_sec) +
                       (now.tv_nsec - start.tv_nsec) / 1000000000.0;
      if (elapsed >= opts->duration_s)
        break;
    }

    if (!conn) {
      conn = http_connect(opts->urls[i % opts->url_count], opts->insecure,
                          opts->timeout);
      if (!conn)
        continue;
    }

    Result res = http_send(conn, opts->urls[i % opts->url_count], opts->method,
                           opts->headers, opts->header_count, opts->body);

    pthread_mutex_lock(ctx->mutex);
    // Dynamic resizing or pre-allocated max?
    // For now, let's assume pre-allocated max or we might need a better
    // structure for duration-based stats. Fixed: stats.c calculate_stats needs
    // an array. Optimization: for duration based, just aggregate counters if
    // it's too many? Let's stick to the current results array for now, but cap
    // it to avoid OOM in long runs.
    if (*(ctx->results_count) < opts->requests || opts->requests == 0) {
      // We'll need to allocate enough space in attacker_run
      ctx->results[*(ctx->results_count)] = res;
      (*(ctx->results_count))++;
    }
    pthread_mutex_unlock(ctx->mutex);

    if (opts->rps > 0) {
      usleep(1000000 / opts->rps);
    }
  }

  if (conn)
    http_close(conn);
  return NULL;
}

void attacker_run(Options *opts) {
  http_init_openssl();

  // Load files if needed
  if (opts->urls_file) {
    opts->url_count = storage_load_urls(opts->urls_file, opts->urls, MAX_URLS);
    if (opts->url_count == 0) {
      ui_error("Error: URLs file is empty or missing.\n");
      return;
    }
  }
  if (opts->body_file) {
    opts->body = storage_read_file(opts->body_file);
  }

  int max_results = opts->requests;
  if (max_results == 0)
    max_results = 1000000; // Cap for duration based at 1M results for now

  pthread_t *threads = malloc(sizeof(pthread_t) * opts->concurrency);
  WorkerContext *contexts = malloc(sizeof(WorkerContext) * opts->concurrency);
  Result *results = malloc(sizeof(Result) * max_results);
  int results_count = 0;
  int stop = 0;
  pthread_mutex_t mutex;
  pthread_mutex_init(&mutex, NULL);

  struct timespec start_time, end_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);

  for (int i = 0; i < opts->concurrency; i++) {
    contexts[i].opts = opts;
    contexts[i].results = results;
    contexts[i].results_count = &results_count;
    contexts[i].mutex = &mutex;
    contexts[i].stop = &stop;
    contexts[i].worker_id = i;
    pthread_create(&threads[i], NULL, worker_thread, &contexts[i]);
  }

  int expected = opts->duration_s > 0 ? 0 : opts->requests;

  while ((expected > 0 ? results_count < expected : 1) && !stop) {
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed = (end_time.tv_sec - start_time.tv_sec) +
                     (end_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;

    if (opts->duration_s > 0) {
      if (elapsed >= opts->duration_s) {
        stop = 1;
        break;
      }
      ui_progress_bar(results_count, 0,
                      elapsed); // Special mode for duration in UI?
    } else {
      ui_progress_bar(results_count, expected, elapsed);
    }
    usleep(100000);
  }
  printf("\n");

  for (int i = 0; i < opts->concurrency; i++) {
    pthread_join(threads[i], NULL);
  }

  clock_gettime(CLOCK_MONOTONIC, &end_time);
  double total_duration =
      (end_time.tv_sec - start_time.tv_sec) +
      (end_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;

  Stats s = calculate_stats(results, results_count, total_duration);
  ui_display_summary(s);
  save_run(opts->urls[0], s.total_requests, s.success, s.failed, s.avg_latency,
           s.rps);

  pthread_mutex_destroy(&mutex);
  free(threads);
  free(contexts);
  free(results);
  http_cleanup_openssl();
}
