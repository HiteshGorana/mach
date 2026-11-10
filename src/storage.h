#ifndef STORAGE_H
#define STORAGE_H

#include "blitz.h"
#include "stats.h"

void storage_init();
void save_run(const char *url, int requests, int success, int failed,
              double avg_latency, double rps);
char *storage_read_file(const char *filename);
int storage_load_urls(const char *filename, char **urls, int max_urls);
int storage_list_history(char **files, int max_files);
void storage_save_tagged(const char *tag, const char *type, Stats stats);
int storage_load_tagged(const char *tag, const char *type, Stats *stats);
void storage_clear_history();
int storage_write_summary(const char *path, const char *url, Stats stats);

#endif
