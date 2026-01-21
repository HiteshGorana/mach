#ifndef STATS_H
#define STATS_H

#include "blitz.h"

typedef struct {
  int total_requests;
  int success;
  int failed;
  double avg_latency;
  double min_latency;
  double max_latency;
  double p50_latency;
  double p95_latency;
  double p99_latency;
  int status_codes[600];
  double rps;
  double total_duration_s;
} Stats;

Stats calculate_stats(Result *results, int count, double total_duration_s);
void display_stats(Stats stats);

#endif
