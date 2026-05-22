#include "storage.h"
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *storage_home() {
  const char *home = getenv("HOME");
#ifdef _WIN32
  if (!home || home[0] == '\0')
    home = getenv("USERPROFILE");
#endif
  return (home && home[0] != '\0') ? home : ".";
}

static void sanitize_component(const char *input, char *output,
                               size_t output_size) {
  size_t j = 0;
  for (size_t i = 0; input[i] != '\0' && j + 1 < output_size; i++) {
    unsigned char c = (unsigned char)input[i];
    output[j++] = (isalnum(c) || c == '-' || c == '_' || c == '.') ? c : '_';
  }
  output[j] = '\0';
}

static void write_json_string(FILE *f, const char *value) {
  fputc('"', f);
  for (const char *p = value; *p != '\0'; p++) {
    if (*p == '"' || *p == '\\') {
      fputc('\\', f);
    }
    fputc(*p, f);
  }
  fputc('"', f);
}

static void ensure_dir(const char *path) {
  struct stat st = {0};
  if (stat(path, &st) == -1) {
#ifdef _WIN32
    mkdir(path);
#else
    mkdir(path, 0755);
#endif
  }
}

void storage_init() {
  char path[512];
  const char *home = storage_home();
  snprintf(path, sizeof(path), "%s/.mach", home);
  ensure_dir(path);
  snprintf(path, sizeof(path), "%s/.mach/history", home);
  ensure_dir(path);
  snprintf(path, sizeof(path), "%s/.mach/tags", home);
  ensure_dir(path);
}

void save_run(const char *url, int requests, int success, int failed,
              double avg_latency, double rps) {
  char filename[512];
  time_t now = time(NULL);
  struct tm *t = localtime(&now);

  char timestamp[32];
  strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", t);

  snprintf(filename, sizeof(filename), "%s/.mach/history/%s.json",
           storage_home(), timestamp);

  FILE *f = fopen(filename, "w");
  if (!f)
    return;

  fprintf(f, "{\n");
  fprintf(f, "  \"timestamp\": \"%lld\",\n", (long long)now);
  fprintf(f, "  \"url\": ");
  write_json_string(f, url);
  fprintf(f, ",\n");
  fprintf(f, "  \"total_requests\": %d,\n", requests);
  fprintf(f, "  \"successful\": %d,\n", success);
  fprintf(f, "  \"failed\": %d,\n", failed);
  fprintf(f, "  \"avg_latency_ms\": %.2f,\n", avg_latency);
  fprintf(f, "  \"rps\": %.2f\n", rps);
  fprintf(f, "}\n");

  fclose(f);
}

char *storage_read_file(const char *filename) {
  FILE *f = fopen(filename, "rb");
  if (!f)
    return NULL;
  if (fseek(f, 0, SEEK_END) != 0) {
    fclose(f);
    return NULL;
  }
  long length = ftell(f);
  if (length < 0 || fseek(f, 0, SEEK_SET) != 0) {
    fclose(f);
    return NULL;
  }
  char *buffer = malloc(length + 1);
  if (buffer) {
    size_t read = fread(buffer, 1, length, f);
    buffer[read] = '\0';
  }
  fclose(f);
  return buffer;
}

int storage_load_urls(const char *filename, char **urls, int max_urls) {
  FILE *f = fopen(filename, "r");
  if (!f)
    return 0;
  char line[1024];
  int count = 0;
  while (fgets(line, sizeof(line), f) && count < max_urls) {
    line[strcspn(line, "\r\n")] = '\0';
    char *start = line;
    while (isspace((unsigned char)*start))
      start++;

    char *end = start + strlen(start);
    while (end > start && isspace((unsigned char)*(end - 1)))
      *--end = '\0';

    if (start[0] != '\0' && start[0] != '#') {
      char *url = strdup(start);
      if (!url)
        break;
      urls[count++] = url;
    }
  }
  fclose(f);
  return count;
}

int storage_list_history(char **files, int max_files) {
  char path[512];
  snprintf(path, sizeof(path), "%s/.mach/history", storage_home());
  DIR *d = opendir(path);
  if (!d)
    return 0;
  struct dirent *dir;
  int count = 0;
  while ((dir = readdir(d)) != NULL && count < max_files) {
    if (strstr(dir->d_name, ".json")) {
      files[count++] = strdup(dir->d_name);
    }
  }
  closedir(d);
  return count;
}

void storage_clear_history() {
  char path[512];
  snprintf(path, sizeof(path), "%s/.mach/history", storage_home());
  DIR *d = opendir(path);
  if (!d)
    return;
  struct dirent *dir;
  while ((dir = readdir(d)) != NULL) {
    if (strstr(dir->d_name, ".json")) {
      char filepath[1024];
      snprintf(filepath, sizeof(filepath), "%s/%s", path, dir->d_name);
      remove(filepath);
    }
  }
  closedir(d);
}

void storage_save_tagged(const char *tag, const char *type, Stats stats) {
  char safe_tag[128];
  sanitize_component(tag, safe_tag, sizeof(safe_tag));

  char path[512];
  snprintf(path, sizeof(path), "%s/.mach/tags/%s", storage_home(), safe_tag);
  ensure_dir(path);

  char filename[512];
  snprintf(filename, sizeof(filename), "%s/%s.json", path, type);

  FILE *f = fopen(filename, "w");
  if (!f)
    return;

  fprintf(f, "{\n");
  fprintf(f, "  \"total_requests\": %d,\n", stats.total_requests);
  fprintf(f, "  \"success\": %d,\n", stats.success);
  fprintf(f, "  \"failed\": %d,\n", stats.failed);
  fprintf(f, "  \"avg_latency\": %.4f,\n", stats.avg_latency);
  fprintf(f, "  \"min_latency\": %.4f,\n", stats.min_latency);
  fprintf(f, "  \"max_latency\": %.4f,\n", stats.max_latency);
  fprintf(f, "  \"p50_latency\": %.4f,\n", stats.p50_latency);
  fprintf(f, "  \"p95_latency\": %.4f,\n", stats.p95_latency);
  fprintf(f, "  \"p99_latency\": %.4f,\n", stats.p99_latency);
  fprintf(f, "  \"rps\": %.4f,\n", stats.rps);
  fprintf(f, "  \"total_duration_s\": %.4f\n", stats.total_duration_s);
  fprintf(f, "}\n");

  fclose(f);
}

int storage_load_tagged(const char *tag, const char *type, Stats *stats) {
  char safe_tag[128];
  sanitize_component(tag, safe_tag, sizeof(safe_tag));

  char filename[512];
  snprintf(filename, sizeof(filename), "%s/.mach/tags/%s/%s.json",
           storage_home(), safe_tag, type);

  char *data = storage_read_file(filename);
  if (!data)
    return 0;

  sscanf(data,
         "{\n"
         "  \"total_requests\": %d,\n"
         "  \"success\": %d,\n"
         "  \"failed\": %d,\n"
         "  \"avg_latency\": %lf,\n"
         "  \"min_latency\": %lf,\n"
         "  \"max_latency\": %lf,\n"
         "  \"p50_latency\": %lf,\n"
         "  \"p95_latency\": %lf,\n"
         "  \"p99_latency\": %lf,\n"
         "  \"rps\": %lf,\n"
         "  \"total_duration_s\": %lf\n"
         "}",
         &stats->total_requests, &stats->success, &stats->failed,
         &stats->avg_latency, &stats->min_latency, &stats->max_latency,
         &stats->p50_latency, &stats->p95_latency, &stats->p99_latency,
         &stats->rps, &stats->total_duration_s);

  free(data);
  return 1;
}
