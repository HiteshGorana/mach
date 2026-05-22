#include "attacker.h"
#include "blitz.h"
#include "http.h"
#include "stats.h"
#include "storage.h"
#include "ui.h"
#include "updater.h"
#include "url.h"
#include <errno.h>
#include <getopt.h>
#include <limits.h>

void print_usage() {
  printf("⚡ Mach\n\n");
  printf("Usage: mach [command] [options] <url>\n\n");
  printf("Commands:\n");
  printf("  attack      Full-featured load test\n");
  printf("  dashboard   View historical test runs\n");
  printf("  history     history clear, history list\n");
  printf("  update      Update Mach to the latest version\n");
  printf("  examples    Show comprehensive usage examples\n");
  printf("  version     Show version information\n");
  printf("\nOptions:\n");
  printf("  -n INT      Total requests (default 100)\n");
  printf("  -d STR      Run duration (e.g., 30s, 1m, 5m)\n");
  printf("  -c INT      Concurrent workers (default 10)\n");
  printf("  -r INT      Requests per second limit\n");
  printf("  -p STR      Test profile (smoke, stress, soak)\n");
  printf("  -m STR      HTTP method (default GET)\n");
  printf("  --tag STR   Tag name for comparison\n");
  printf("  --before    Set as baseline for tag\n");
  printf("  --after     Set as target for tag comparison\n");
  printf("  --result    Show comparison result for tag\n");
  printf("  --threshold FLOAT Max allowed regression %% (default 0)\n");
}

static int parse_int_arg(const char *value, const char *name, int min_value,
                         int *out) {
  errno = 0;
  char *end = NULL;
  long parsed = strtol(value, &end, 10);
  if (errno != 0 || end == value || *end != '\0' || parsed < min_value ||
      parsed > INT_MAX) {
    fprintf(stderr, "Error: %s must be an integer >= %d\n", name, min_value);
    return 0;
  }
  *out = (int)parsed;
  return 1;
}

static int parse_double_arg(const char *value, const char *name,
                            double min_value, double *out) {
  errno = 0;
  char *end = NULL;
  double parsed = strtod(value, &end);
  if (errno != 0 || end == value || *end != '\0' || parsed < min_value) {
    fprintf(stderr, "Error: %s must be a number >= %.0f\n", name, min_value);
    return 0;
  }
  *out = parsed;
  return 1;
}

static int parse_duration_arg(const char *value, const char *name, int *out) {
  errno = 0;
  char *end = NULL;
  long parsed = strtol(value, &end, 10);
  if (errno != 0 || end == value || parsed <= 0) {
    fprintf(stderr, "Error: %s must be a positive duration\n", name);
    return 0;
  }

  long multiplier = 1;
  if (*end == '\0' || strcmp(end, "s") == 0) {
    multiplier = 1;
  } else if (strcmp(end, "m") == 0) {
    multiplier = 60;
  } else if (strcmp(end, "h") == 0) {
    multiplier = 3600;
  } else {
    fprintf(stderr, "Error: %s must use s, m, or h suffix\n", name);
    return 0;
  }

  if (parsed > INT_MAX / multiplier) {
    fprintf(stderr, "Error: %s is too large\n", name);
    return 0;
  }

  *out = (int)(parsed * multiplier);
  return 1;
}

static char *copy_arg(const char *value, const char *name) {
  char *copy = strdup(value);
  if (!copy) {
    fprintf(stderr, "Error: could not copy %s\n", name);
  }
  return copy;
}

static int add_header(Options *opts, const char *value) {
  if (opts->header_count >= MAX_HEADERS) {
    fprintf(stderr, "Error: too many headers, max is %d\n", MAX_HEADERS);
    return 0;
  }

  const char *colon = strchr(value, ':');
  if (!colon || colon == value || colon[1] == '\0') {
    fprintf(stderr, "Error: header must use Key:Value format\n");
    return 0;
  }

  size_t key_len = (size_t)(colon - value);
  size_t value_len = strlen(colon + 1);
  if (key_len >= sizeof(opts->headers[0].key) ||
      value_len >= sizeof(opts->headers[0].value)) {
    fprintf(stderr, "Error: header is too long\n");
    return 0;
  }

  Header *header = &opts->headers[opts->header_count];
  memcpy(header->key, value, key_len);
  header->key[key_len] = '\0';
  memcpy(header->value, colon + 1, value_len + 1);
  opts->header_count++;
  return 1;
}

static int apply_profile(Options *opts, const char *profile) {
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
  } else {
    fprintf(stderr, "Error: unknown profile '%s'\n", profile);
    return 0;
  }
  return 1;
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

  if (argc > 1 && strcmp(argv[1], "update") == 0) {
    http_init_openssl();
    updater_run();
    http_cleanup_openssl();
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
  int requests_set = 0;

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
      {"tag", required_argument, 0, 1004},
      {"before", no_argument, 0, 1005},
      {"after", no_argument, 0, 1006},
      {"result", no_argument, 0, 1007},
      {"threshold", required_argument, 0, 1008},
      {"version", no_argument, 0, 'v'},
      {"help", no_argument, 0, '?'},
      {0, 0, 0, 0}};

  int opt;
  optind = start_idx;
  while ((opt = getopt_long(argc, argv, "n:d:c:r:p:m:h:b:t:kv?", long_options,
                            NULL)) != -1) {
    switch (opt) {
    case 'n':
      if (!parse_int_arg(optarg, "--requests", 1, &opts.requests))
        return 1;
      requests_set = 1;
      break;
    case 'd':
      if (!parse_duration_arg(optarg, "--duration", &opts.duration_s))
        return 1;
      break;
    case 'c':
      if (!parse_int_arg(optarg, "--concurrency", 1, &opts.concurrency))
        return 1;
      break;
    case 'r':
      if (!parse_int_arg(optarg, "--rps", 0, &opts.rps))
        return 1;
      break;
    case 'p':
      if (!apply_profile(&opts, optarg))
        return 1;
      break;
    case 1003: {
      int seconds = 0;
      if (!parse_duration_arg(optarg, "--ramp-up", &seconds))
        return 1;
      opts.ramp_up.tv_sec = seconds;
      break;
    }
    case 'm':
      opts.method = copy_arg(optarg, "--method");
      if (!opts.method)
        return 1;
      break;
    case 'h':
      if (!add_header(&opts, optarg))
        return 1;
      break;
    case 'b':
      opts.body = copy_arg(optarg, "--body");
      if (!opts.body)
        return 1;
      break;
    case 1001:
      opts.body_file = copy_arg(optarg, "--body-file");
      if (!opts.body_file)
        return 1;
      break;
    case 1002:
      opts.urls_file = copy_arg(optarg, "--urls-file");
      if (!opts.urls_file)
        return 1;
      break;
    case 't': {
      int seconds = 0;
      if (!parse_duration_arg(optarg, "--timeout", &seconds))
        return 1;
      opts.timeout.tv_sec = seconds;
      break;
    }
    case 'k':
      opts.insecure = 1;
      break;
    case 1004:
      opts.tag = copy_arg(optarg, "--tag");
      if (!opts.tag)
        return 1;
      break;
    case 1005:
      opts.before = 1;
      break;
    case 1006:
      opts.after = 1;
      break;
    case 1007:
      opts.show_result = 1;
      break;
    case 1008:
      if (!parse_double_arg(optarg, "--threshold", 0, &opts.threshold))
        return 1;
      break;
    case 'v':
      printf("Mach v%s\n", VERSION);
      return 0;
    case '?':
      print_usage();
      return 0;
    }
  }

  if (opts.duration_s > 0 && !requests_set && opts.requests == 100) {
    opts.requests = 0;
  }

  if (opts.before && opts.after) {
    ui_error("Error: --before and --after cannot be used together\n");
    return 1;
  }

  if ((opts.before || opts.after || opts.threshold > 0 || opts.show_result) &&
      !opts.tag) {
    ui_error("Error: tag options require --tag <name>\n");
    return 1;
  }

  if (opts.threshold > 0 && !opts.after) {
    ui_error("Error: --threshold is only valid with --after\n");
    return 1;
  }

  if (opts.show_result) {
    ui_display_comparison(opts.tag);
    return 0;
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

  for (int i = 0; i < opts.url_count; i++) {
    if (!url_is_supported(opts.urls[i])) {
      ui_error("Error: Unsupported or invalid URL: ");
      printf("%s\n", opts.urls[i]);
      return 1;
    }
  }

  attacker_run(&opts);

  return 0;
}
