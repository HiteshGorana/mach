#include "stats.h"
#include <float.h>
#include <math.h>

// ASM-optimized functions
extern double fast_sum(double *data, int count);
extern double fast_min(double *data, int count);
extern double fast_max(double *data, int count);

static int compare_doubles(const void *a, const void *b) {
  double da = *(const double *)a;
  double db = *(const double *)b;
  return (da > db) - (da < db);
}

Stats calculate_stats(Result *results, int count, double total_duration_s) {
  Stats s = {0};
  s.total_requests = count;
  s.total_duration_s = total_duration_s;

  if (count == 0) {
    return s;
  }

  double *latencies = malloc(sizeof(double) * count);
  int valid_latencies = 0;

  for (int i = 0; i < count; i++) {
    if (results[i].status_code >= 200 && results[i].status_code < 400) {
      s.success++;
    } else {
      s.failed++;
    }

    if (results[i].status_code > 0 && results[i].status_code < 600) {
      s.status_codes[results[i].status_code]++;
    }

    if (results[i].duration_ms > 0) {
      latencies[valid_latencies++] = results[i].duration_ms;
    }
  }

  if (valid_latencies > 0) {
    // Use ASM-optimized NEON functions
    double sum_latency = fast_sum(latencies, valid_latencies);
    s.avg_latency = sum_latency / valid_latencies;
    s.min_latency = fast_min(latencies, valid_latencies);
    s.max_latency = fast_max(latencies, valid_latencies);

    qsort(latencies, valid_latencies, sizeof(double), compare_doubles);
    s.p50_latency = latencies[(int)(valid_latencies * 0.50)];
    s.p95_latency = latencies[(int)(valid_latencies * 0.95)];
    s.p99_latency = latencies[(int)(valid_latencies * 0.99)];
  }

  if (total_duration_s > 0) {
    s.rps = count / total_duration_s;
  }

  free(latencies);
  return s;
}
