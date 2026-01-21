#ifndef BLITZ_H
#define BLITZ_H

// Platform-specific includes
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#endif

#include <pthread.h>

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VERSION "1.1.0"
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
  char *tag;
  int before;
  int after;
  int show_result;
  double threshold;
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
