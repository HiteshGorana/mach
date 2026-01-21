#ifndef UI_H
#define UI_H

#include "stats.h"

void ui_header(const char *text);
void ui_info(const char *text);
void ui_success(const char *text);
void ui_error(const char *text);
void ui_progress_bar(int current, int total, double elapsed_s);
void ui_display_summary(Stats s);
void ui_dashboard();
void ui_examples();

#endif
