#include "attacker.h"
#include "stats.h"
#include "storage.h"
#include "ui.h"
#include "url.h"
#include <unistd.h>

#define COLOR_RESET "\x1b[0m"
#define COLOR_RED "\x1b[31m"

static int should_stop(WorkerContext *ctx) {
  pthread_mutex_lock(ctx->mutex);
  int stop = *(ctx->stop);
  pthread_mutex_unlock(ctx->mutex);
  return stop;
}

static void request_stop(WorkerContext *ctx) {
  pthread_mutex_lock(ctx->mutex);
  *(ctx->stop) = 1;
  pthread_mutex_unlock(ctx->mutex);
}

static void record_result(WorkerContext *ctx, Result res) {
  pthread_mutex_lock(ctx->mutex);
  int count = *(ctx->results_count);
  if (count < ctx->max_results) {
    ctx->results[count] = res;
    *(ctx->results_count) = count + 1;
  } else {
    *(ctx->stop) = 1;
  }
  pthread_mutex_unlock(ctx->mutex);
}

static void read_state(pthread_mutex_t *mutex, int *results_count, int *stop,
                       int *out_count, int *out_stop) {
  pthread_mutex_lock(mutex);
  *out_count = *results_count;
  *out_stop = *stop;
  pthread_mutex_unlock(mutex);
}

static void throttle_global_rps(const Options *opts) {
  if (opts->rps <= 0)
    return;

  double delay_us = (1000000.0 * opts->concurrency) / opts->rps;
  if (delay_us > 0)
    usleep((useconds_t)delay_us);
}

static int can_retry_request(const Options *opts) {
  return strcmp(opts->method, "GET") == 0 || strcmp(opts->method, "HEAD") == 0;
}

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

  for (int i = 0; (total_to_do == -1 || i < total_to_do) && !should_stop(ctx);
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
      if (!conn) {
        Result res = {.url = opts->urls[i % opts->url_count],
                      .duration_ms = 0,
                      .status_code = 0,
                      .error = "Connection failed"};
        record_result(ctx, res);
        throttle_global_rps(opts);
        continue;
      }
    }

    Result res = http_send(conn, opts->urls[i % opts->url_count], opts->method,
                           opts->headers, opts->header_count, opts->body);

    if (res.status_code == 0 && can_retry_request(opts)) {
      http_close(conn);
      conn = NULL;
      conn = http_connect(opts->urls[i % opts->url_count], opts->insecure,
                          opts->timeout);
      if (conn) {
        res = http_send(conn, opts->urls[i % opts->url_count], opts->method,
                        opts->headers, opts->header_count, opts->body);
      }
    }

    record_result(ctx, res);

    if (res.status_code == 0 && conn) {
      http_close(conn);
      conn = NULL;
    }

    throttle_global_rps(opts);
  }

  if (conn)
    http_close(conn);
  return NULL;
}

void attacker_run(Options *opts) {
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
    if (!opts->body) {
      ui_error("Error: Could not read body file.\n");
      return;
    }
  }

  for (int i = 0; i < opts->url_count; i++) {
    if (!url_is_supported(opts->urls[i])) {
      ui_error("Error: Unsupported or invalid URL: ");
      printf("%s\n", opts->urls[i]);
      return;
    }
  }

  int max_results = opts->requests > 0 ? opts->requests : 1000000;

  pthread_t *threads = malloc(sizeof(pthread_t) * opts->concurrency);
  WorkerContext *contexts = malloc(sizeof(WorkerContext) * opts->concurrency);
  Result *results = malloc(sizeof(Result) * max_results);
  if (!threads || !contexts || !results) {
    ui_error("Error: Not enough memory to start load test.\n");
    free(threads);
    free(contexts);
    free(results);
    return;
  }

  int results_count = 0;
  int stop = 0;
  pthread_mutex_t mutex;
  if (pthread_mutex_init(&mutex, NULL) != 0) {
    ui_error("Error: Could not initialize worker lock.\n");
    free(threads);
    free(contexts);
    free(results);
    return;
  }

  http_init_openssl();

  struct timespec start_time, end_time;
  clock_gettime(CLOCK_MONOTONIC, &start_time);

  int created_threads = 0;
  for (int i = 0; i < opts->concurrency; i++) {
    contexts[i].opts = opts;
    contexts[i].results = results;
    contexts[i].results_count = &results_count;
    contexts[i].max_results = max_results;
    contexts[i].mutex = &mutex;
    contexts[i].stop = &stop;
    contexts[i].worker_id = i;
    if (pthread_create(&threads[i], NULL, worker_thread, &contexts[i]) != 0) {
      request_stop(&contexts[i]);
      ui_error("Error: Could not create all worker threads.\n");
      break;
    }
    created_threads++;
  }

  if (created_threads == 0) {
    pthread_mutex_destroy(&mutex);
    free(threads);
    free(contexts);
    free(results);
    http_cleanup_openssl();
    return;
  }

  int expected = opts->requests;

  while (1) {
    int current_count = 0;
    int current_stop = 0;
    read_state(&mutex, &results_count, &stop, &current_count, &current_stop);
    if (current_stop || (expected > 0 && current_count >= expected))
      break;

    clock_gettime(CLOCK_MONOTONIC, &end_time);
    double elapsed = (end_time.tv_sec - start_time.tv_sec) +
                     (end_time.tv_nsec - start_time.tv_nsec) / 1000000000.0;

    if (opts->duration_s > 0) {
      if (elapsed >= opts->duration_s) {
        pthread_mutex_lock(&mutex);
        stop = 1;
        pthread_mutex_unlock(&mutex);
        break;
      }
      ui_progress_bar(current_count, expected, elapsed);
    } else {
      ui_progress_bar(current_count, expected, elapsed);
    }
    usleep(100000);
  }
  printf("\n");

  for (int i = 0; i < created_threads; i++) {
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

  if (opts->tag) {
    if (opts->before) {
      storage_save_tagged(opts->tag, "before", s);
      ui_success("   [TAGGED as before]\n");
    } else if (opts->after) {
      storage_save_tagged(opts->tag, "after", s);
      ui_success("   [TAGGED as after]\n");

      // Automated Threshold Check
      if (opts->threshold > 0) {
        Stats before;
        if (storage_load_tagged(opts->tag, "before", &before)) {
          double diff = s.avg_latency - before.avg_latency;
          double pct = (diff / before.avg_latency) * 100.0;
          if (pct > opts->threshold) {
            printf("\n%s❌ REGRESSION DETECTED: %.1f%% (Threshold: %.1f%%)%s\n",
                   COLOR_RED, pct, opts->threshold, COLOR_RESET);
            exit(1);
          }
        }
      }
    }
  }

  pthread_mutex_destroy(&mutex);
  free(threads);
  free(contexts);
  free(results);
  http_cleanup_openssl();
}
