#include "attacker.h"
#include "blitz.h"
#include "http.h"
#include "stats.h"
#include "storage.h"
#include "ui.h"
#include <getopt.h>

void print_usage() {
  printf("⚡ Mach\n\n");
  printf("Usage: mach [command] [options] <url>\n\n");
  printf("Commands:\n");
  printf("  attack      Full-featured load test\n");
  printf("  dashboard   View historical test runs\n");
  printf("  history     history clear, history list\n");
  printf("  examples    Show comprehensive usage examples\n");
  printf("  version     Show version information\n");
  printf("\nOptions:\n");
  printf("  -n INT      Total requests (default 100)\n");
  printf("  -d STR      Run duration (e.g., 30s, 1m, 5m)\n");
  printf("  -c INT      Concurrent workers (default 10)\n");
  printf("  -r INT      Requests per second limit\n");
  printf("  -p STR      Test profile (smoke, stress, soak)\n");
  printf("  -m STR      HTTP method (default GET)\n");
}

static int parse_duration(const char *dur) {
  int val = atoi(dur);
  if (strstr(dur, "s"))
    return val;
  if (strstr(dur, "m"))
    return val * 60;
  if (strstr(dur, "h"))
    return val * 3600;
  return val;
}

static void apply_profile(Options *opts, const char *profile) {
  if (strcmp(profile, "smoke") == 0) {
    opts->requests = 10;
    opts->concurrency = 2;
  } else if (strcmp(profile, "stress") == 0) {
    opts->requests = 10000;
    opts->concurrency = 100;
  } else if (strcmp(profile, "soak") == 0) {
    opts->duration_s = 300; // 5 min
    opts->concurrency = 50;
    opts->requests = 0;
  }
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    print_usage();
    return 1;
  }

  storage_init();

  if (strcmp(argv[1], "dashboard") == 0 || strcmp(argv[1], "dash") == 0) {
    ui_dashboard();
    return 0;
  }

  if (strcmp(argv[1], "history") == 0) {
    if (argc > 2 && strcmp(argv[2], "clear") == 0) {
      storage_clear_history();
      ui_success("History cleared.\n");
      return 0;
    }
  }

  if (strcmp(argv[1], "examples") == 0) {
    ui_examples();
    return 0;
  }

  if (strcmp(argv[1], "version") == 0) {
    printf("Mach v%s\n", VERSION);
    return 0;
  }

  int start_idx = 1;
  if (strcmp(argv[1], "attack") == 0) {
    start_idx = 2;
  }

  Options opts = {0};
  opts.method = "GET";
  opts.requests = 100;
  opts.concurrency = 10;
  opts.timeout.tv_sec = 10;

  static struct option long_options[] = {
      {"requests", required_argument, 0, 'n'},
      {"duration", required_argument, 0, 'd'},
      {"concurrency", required_argument, 0, 'c'},
      {"rps", required_argument, 0, 'r'},
      {"profile", required_argument, 0, 'p'},
      {"method", required_argument, 0, 'm'},
      {"header", required_argument, 0, 'h'},
      {"body", required_argument, 0, 'b'},
      {"body-file", required_argument, 0, 1001},
      {"urls-file", required_argument, 0, 1002},
      {"ramp-up", required_argument, 0, 1003},
      {"timeout", required_argument, 0, 't'},
      {"insecure", no_argument, 0, 'k'},
      {"version", no_argument, 0, 'v'},
      {"help", no_argument, 0, '?'},
      {0, 0, 0, 0}};

  int opt;
  optind = start_idx;
  while ((opt = getopt_long(argc, argv, "n:d:c:r:p:m:h:b:t:kv?", long_options,
                            NULL)) != -1) {
    switch (opt) {
    case 'n':
      opts.requests = atoi(optarg);
      break;
    case 'd':
      opts.duration_s = parse_duration(optarg);
      break;
    case 'c':
      opts.concurrency = atoi(optarg);
      break;
    case 'r':
      opts.rps = atoi(optarg);
      break;
    case 'p':
      apply_profile(&opts, optarg);
      break;
    case 1003:
      opts.ramp_up.tv_sec = parse_duration(optarg);
      break;
    case 'm':
      opts.method = strdup(optarg);
      break;
    case 'h': {
      char *colon = strchr(optarg, ':');
      if (colon && opts.header_count < MAX_HEADERS) {
        *colon = '\0';
        strncpy(opts.headers[opts.header_count].key, optarg, 127);
        strncpy(opts.headers[opts.header_count].value, colon + 1, 511);
        opts.header_count++;
      }
      break;
    }
    case 'b':
      opts.body = strdup(optarg);
      break;
    case 1001:
      opts.body_file = strdup(optarg);
      break;
    case 1002:
      opts.urls_file = strdup(optarg);
      break;
    case 't':
      opts.timeout.tv_sec = atoi(optarg);
      break;
    case 'k':
      opts.insecure = 1;
      break;
    case 'v':
      printf("Mach v%s\n", VERSION);
      return 0;
    case '?':
      print_usage();
      return 0;
    }
  }

  if (optind < argc) {
    opts.urls[opts.url_count++] = argv[optind];
  } else if (!opts.urls_file) {
    if (argc > start_idx && argv[argc - 1][0] != '-') {
      opts.urls[opts.url_count++] = argv[argc - 1];
    } else {
      print_usage();
      return 1;
    }
  }

  attacker_run(&opts);

  return 0;
}
