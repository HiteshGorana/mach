#include "storage.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

static void ensure_dir(const char *path) {
  struct stat st = {0};
  if (stat(path, &st) == -1) {
    mkdir(path, 0755);
  }
}

void storage_init() {
  char path[512];
  snprintf(path, sizeof(path), "%s/.mach", getenv("HOME"));
  ensure_dir(path);
  snprintf(path, sizeof(path), "%s/.mach/history", getenv("HOME"));
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
           getenv("HOME"), timestamp);

  FILE *f = fopen(filename, "w");
  if (!f)
    return;

  fprintf(f, "{\n");
  fprintf(f, "  \"timestamp\": \"%ld\",\n", now);
  fprintf(f, "  \"url\": \"%s\",\n", url);
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
  fseek(f, 0, SEEK_END);
  long length = ftell(f);
  fseek(f, 0, SEEK_SET);
  char *buffer = malloc(length + 1);
  if (buffer) {
    fread(buffer, 1, length, f);
    buffer[length] = '\0';
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
    size_t len = strlen(line);
    if (len > 0 && line[len - 1] == '\n')
      line[len - 1] = '\0';
    if (strlen(line) > 0 && line[0] != '#') {
      urls[count++] = strdup(line);
    }
  }
  fclose(f);
  return count;
}

int storage_list_history(char **files, int max_files) {
  char path[512];
  snprintf(path, sizeof(path), "%s/.blitz/history", getenv("HOME"));
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
  snprintf(path, sizeof(path), "%s/.blitz/history", getenv("HOME"));
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
