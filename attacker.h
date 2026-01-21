#ifndef ATTACKER_H
#define ATTACKER_H

#include "blitz.h"
#include "http.h"

typedef struct {
  Options *opts;
  Result *results;
  int *results_count;
  pthread_mutex_t *mutex;
  int *stop;
  int worker_id;
} WorkerContext;

void attacker_run(Options *opts);

#endif
