#include "ui.h"
#include "storage.h"
#include "terminal.h"
#include <stdio.h>
#include <string.h>

#define COLOR_RESET "\x1b[0m"
#define COLOR_BOLD "\x1b[1m"
#define COLOR_DIM "\x1b[2m"
#define COLOR_GREEN "\x1b[32m"
#define COLOR_RED "\x1b[31m"
#define COLOR_CYAN "\x1b[36m"
#define COLOR_YELLOW "\x1b[33m"
#define COLOR_BRIGHT_YELLOW "\x1b[93m"
#define COLOR_MAGENTA "\x1b[35m"
#define COLOR_BRIGHT_MAGENTA "\x1b[95m"

void ui_header(const char *text) {
  printf("%s%s%s\n", COLOR_BOLD COLOR_BRIGHT_MAGENTA, text, COLOR_RESET);
}

void ui_info(const char *text) {
  printf("%s%s%s", COLOR_CYAN, text, COLOR_RESET);
}

void ui_success(const char *text) {
  printf("%s%s%s", COLOR_GREEN, text, COLOR_RESET);
}

void ui_error(const char *text) {
  printf("%s%s%s", COLOR_RED, text, COLOR_RESET);
}

// Status codes are displayed simply in the compact view.

void ui_progress_bar(int current, int total, double elapsed_s) {
  if (total == 0) {
    printf("\r  ⏱️  %.1fs | %d req | %.1f/s ", elapsed_s, current,
           elapsed_s > 0 ? (double)current / elapsed_s : 0);
    fflush(stdout);
    return;
  }
  int width = 20;
  int filled = (int)((double)current / total * width);
  if (filled > width)
    filled = width;

  printf("\r  %s[", COLOR_CYAN);
  for (int i = 0; i < filled; i++)
    printf("█");
  for (int i = filled; i < width; i++)
    printf("░");
  printf("]%s %d/%d (%.0f%%) ", COLOR_RESET, current, total,
         (double)current / total * 100);
  fflush(stdout);
}

void ui_display_summary(Stats s) {
  printf("\n\n%s🚀 SUMMARY%s\n", COLOR_BOLD COLOR_BRIGHT_MAGENTA, COLOR_RESET);
  printf("%s──────────────────────────────────%s\n", COLOR_DIM, COLOR_RESET);

  printf("  %-15s %d\n", "Requests", s.total_requests);
  printf("  %-15s %s%d%s\n", "Successful", COLOR_GREEN, s.success, COLOR_RESET);
  printf("  %-15s %s%d%s\n", "Failed", (s.failed > 0 ? COLOR_RED : COLOR_DIM),
         s.failed, COLOR_RESET);

  if (s.total_requests > 0) {
    double pct = (double)s.success / s.total_requests * 100;
    printf("  %-15s %.2f%%\n", "Success Rate", pct);
  }

  printf("  %-15s %.2fs\n", "Duration", s.total_duration_s);
  printf("  %-15s %.2f/s\n", "Requests/sec", s.rps);

  if (s.success > 0) {
    printf("\n%s📊 LATENCY%s\n", COLOR_BOLD COLOR_BRIGHT_MAGENTA, COLOR_RESET);
    printf("  Avg: %.2fms | P50: %.2fms | P95: %.2fms\n", s.avg_latency,
           s.p50_latency, s.p95_latency);
  }

  int printed_status = 0;
  for (int i = 100; i < 600; i++) {
    if (s.status_codes[i] > 0) {
      if (!printed_status) {
        printf("\n%s📡 STATUS%s\n", COLOR_BOLD COLOR_BRIGHT_MAGENTA,
               COLOR_RESET);
        printed_status = 1;
      }
      printf("  %d: %d  ", i, s.status_codes[i]);
    }
  }
  printf("\n\n");
}

void ui_dashboard() {
  char *files[100];
  int count = storage_list_history(files, 100);
  if (count == 0) {
    ui_error("No history found.\n");
    return;
  }

  int cursor = 0;
  terminal_make_raw();

  while (1) {
    printf("\033[H\033[J"); // Clear screen
    ui_header("⚡ MACH HISTORY DASHBOARD");
    printf("Use ↑/↓ to navigate, ENTER to view, 'q' to exit\n\n");

    for (int i = 0; i < count; i++) {
      if (i == cursor) {
        printf("%s  ➔ %s%s\n", COLOR_CYAN, files[i], COLOR_RESET);
      } else {
        printf("    %s\n", files[i]);
      }
    }

    int key = terminal_read_key();
    if (key == 'q' || key == 27)
      break;
    if (key == 1000 && cursor > 0)
      cursor--; // UP
    if (key == 1001 && cursor < count - 1)
      cursor++; // DOWN

    if (key == '\n' || key == '\r') {
      char full_path[512];
      snprintf(full_path, sizeof(full_path), "%s/.blitz/history/%s",
               getenv("HOME"), files[cursor]);
      char *content = storage_read_file(full_path);
      if (content) {
        printf("\033[H\033[J");
        ui_header("Run Details");
        printf("%s\n\n", content);
        printf("Press any key to return...");
        fflush(stdout);
        terminal_read_key();
        free(content);
      }
    }
  }

  terminal_restore();
  for (int i = 0; i < count; i++)
    free(files[i]);
}

void ui_examples() {
  ui_header("⚡ MACH USAGE EXAMPLES");
  printf("\n  1. Quick Test:\n");
  printf("     ./mach http://localhost:8080\n");
  printf("\n  2. Custom Load:\n");
  printf("     ./mach -n 1000 -c 50 http://example.com\n");
  printf("\n  3. Soak Test (Duration based):\n");
  printf("     ./mach -d 5m -c 20 http://api.example.com\n");
  printf("\n  4. Stress Test (Profile):\n");
  printf("     ./mach --profile stress http://example.com\n");
  printf("\n  5. POST with JSON body:\n");
  printf("     ./mach -m POST -h \"Content-Type:application/json\" -b "
         "'{\"id\":1}' http://api.com\n");
}
