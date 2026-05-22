#include "url.h"
#include <ctype.h>
#include <stddef.h>
#include <string.h>

static int copy_slice(char *dest, size_t dest_size, const char *start,
                      size_t len) {
  if (len == 0 || len >= dest_size)
    return 0;
  memcpy(dest, start, len);
  dest[len] = '\0';
  return 1;
}

static const char *first_path_char(const char *authority) {
  const char *slash = strchr(authority, '/');
  const char *query = strchr(authority, '?');
  if (!slash)
    return query;
  if (!query)
    return slash;
  return query < slash ? query : slash;
}

int url_parse(const char *url, ParsedUrl *out) {
  if (!url || !out)
    return 0;

  memset(out, 0, sizeof(*out));
  strcpy(out->path, "/");

  const char *scheme_end = strstr(url, "://");
  if (!scheme_end)
    return 0;

  if (!copy_slice(out->protocol, sizeof(out->protocol), url,
                  (size_t)(scheme_end - url))) {
    return 0;
  }

  const char *authority = scheme_end + 3;
  const char *path_start = first_path_char(authority);
  const char *host_end = path_start ? path_start : authority + strlen(authority);
  if (host_end == authority)
    return 0;

  const char *port_sep = NULL;
  for (const char *p = authority; p < host_end; p++) {
    if (*p == ':')
      port_sep = p;
  }

  const char *host_stop = port_sep ? port_sep : host_end;
  if (!copy_slice(out->host, sizeof(out->host), authority,
                  (size_t)(host_stop - authority))) {
    return 0;
  }

  int has_port = 0;
  if (port_sep) {
    const char *port_start = port_sep + 1;
    if (port_start == host_end)
      return 0;

    int port = 0;
    for (const char *p = port_start; p < host_end; p++) {
      if (!isdigit((unsigned char)*p))
        return 0;
      port = port * 10 + (*p - '0');
      if (port > 65535)
        return 0;
    }
    if (port <= 0)
      return 0;
    out->port = port;
    has_port = 1;
  }

  if (strcmp(out->protocol, "https") == 0) {
    out->is_https = 1;
    if (!has_port)
      out->port = 443;
  } else if (strcmp(out->protocol, "http") == 0) {
    out->is_https = 0;
    if (!has_port)
      out->port = 80;
  } else {
    return 0;
  }

  if (path_start) {
    size_t path_len = strlen(path_start);
    if (*path_start == '?') {
      if (path_len + 1 >= sizeof(out->path))
        return 0;
      out->path[0] = '/';
      memcpy(out->path + 1, path_start, path_len + 1);
    } else {
      if (!copy_slice(out->path, sizeof(out->path), path_start, path_len))
        return 0;
    }
  }

  return 1;
}

int url_is_supported(const char *url) {
  ParsedUrl parsed;
  return url_parse(url, &parsed);
}
