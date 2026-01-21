#ifndef BLITZ_H
#define BLITZ_H

#include <arpa/inet.h>
#include <netdb.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define VERSION "1.0.0"
#define MAX_URLS 1024
#define MAX_HEADERS 64

typedef struct {
  char key[128];
  char value[512];
} Header;

typedef struct {
  char *urls[MAX_URLS];
  int url_count;
  char *method;
  Header headers[MAX_HEADERS];
  int header_count;
  char *body;
  int requests;
  int concurrency;
  int rps;
  int duration_s;
  char *body_file;
  char *urls_file;
  char *profile;
  struct timespec ramp_up;
  struct timespec timeout;
  int insecure;
  char *proxy_url;
  char *output_file;
  int quiet;
  int no_color;
} Options;

typedef struct {
  char *url;
  int status_code;
  double duration_ms;
  char *error;
} Result;

void print_usage();
void print_examples();

#endif
