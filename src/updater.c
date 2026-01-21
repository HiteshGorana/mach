#include "updater.h"
#include "blitz.h"
#include "http.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#endif

#ifndef _WIN32
#define PLATFORM_NAME "macos"
#ifdef __linux__
#undef PLATFORM_NAME
#define PLATFORM_NAME "linux"
#endif
#else
#define PLATFORM_NAME "windows"
#endif

#ifndef _WIN32
#define ARCH_NAME "x86_64"
#ifdef __arm64__
#undef ARCH_NAME
#define ARCH_NAME "arm64"
#endif
#ifdef __aarch64__
#undef ARCH_NAME
#define ARCH_NAME "arm64"
#endif
#else
#define ARCH_NAME "x86_64"
#endif

static char *get_current_executable_path() {
  static char path[1024];
#ifdef _WIN32
  GetModuleFileName(NULL, path, sizeof(path));
#elif defined(__APPLE__)
  uint32_t size = sizeof(path);
  if (_NSGetExecutablePath(path, &size) != 0)
    return NULL;
#else
  ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
  if (len != -1) {
    path[len] = '\0';
  } else {
    return NULL;
  }
#endif
  return path;
}

void updater_run() {
  printf("⚡ Checking for updates...\n");

  char *json = http_fetch_body(
      "https://api.github.com/repos/HiteshGorana/mach/releases/latest", 1);
  if (!json) {
    printf("❌ Failed to fetch release info from GitHub.\n");
    return;
  }

  char *tag_ptr = strstr(json, "\"tag_name\":");
  if (!tag_ptr) {
    printf("❌ Could not find version info in GitHub response.\n");
    free(json);
    return;
  }

  char remote_version[32];
  sscanf(tag_ptr, "\"tag_name\":\"v%31[^\"]\"", remote_version);

  if (strcmp(remote_version, VERSION) == 0) {
    printf("✅ Mach is already up to date (v%s).\n", VERSION);
    free(json);
    return;
  }

  printf("🚀 New version found: v%s (Current: v%s)\n", remote_version, VERSION);

  // Find asset for current platform
  char asset_pattern[64];
  snprintf(asset_pattern, sizeof(asset_pattern), "mach-%s-%s", PLATFORM_NAME,
           ARCH_NAME);
#ifdef _WIN32
  strcat(asset_pattern, ".exe");
#endif

  char *asset_ptr = strstr(json, asset_pattern);
  if (!asset_ptr) {
    printf("❌ Could not find a pre-built binary for your platform (%s-%s).\n",
           PLATFORM_NAME, ARCH_NAME);
    free(json);
    return;
  }

  char *download_ptr = strstr(asset_ptr, "\"browser_download_url\":");
  if (!download_ptr) {
    printf("❌ Could not find download URL for asset.\n");
    free(json);
    return;
  }

  char download_url[1024];
  sscanf(download_ptr, "\"browser_download_url\":\"%1023[^\"]\"", download_url);
  free(json);

  char *current_path = get_current_executable_path();
  if (!current_path) {
    printf("❌ Could not determine current executable path.\n");
    return;
  }

  char temp_path[1024];
  snprintf(temp_path, sizeof(temp_path), "%s.tmp", current_path);

  printf("📥 Downloading v%s...\n", remote_version);
  if (http_download_to_file(download_url, temp_path, 1) != 0) {
    printf("❌ Download failed.\n");
    remove(temp_path);
    return;
  }

#ifndef _WIN32
  chmod(temp_path, 0755);
#endif

  printf("🔄 Installing update...\n");

#ifdef _WIN32
  // On Windows, we can't overwrite a running exe.
  // We rename the current exe and then move the new one.
  char old_path[1024];
  snprintf(old_path, sizeof(old_path), "%s.old", current_path);
  remove(old_path);
  if (rename(current_path, old_path) != 0) {
    printf("❌ Failed to rename current executable. Permission issue?\n");
    remove(temp_path);
    return;
  }
  if (rename(temp_path, current_path) != 0) {
    printf("❌ Failed to move new executable into place.\n");
    rename(old_path, current_path); // Try to restore
    return;
  }
#else
  if (rename(temp_path, current_path) != 0) {
    perror("rename");
    printf("❌ Failed to replace binary. Try running with 'sudo'.\n");
    remove(temp_path);
    return;
  }
#endif

  printf("✨ Successfully updated to v%s!\n", remote_version);
}
