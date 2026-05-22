#include "http.h"
#include "url.h"
#include <stdarg.h>
#include <sys/socket.h>

// ASM-optimized functions
extern int fast_parse_status(char *response);
extern double fast_duration_ms(long sec_diff, long nsec_diff);

static int append_request(char *request, size_t request_size, size_t *len,
                          const char *fmt, ...) {
  if (*len >= request_size)
    return 0;

  va_list args;
  va_start(args, fmt);
  int written = vsnprintf(request + *len, request_size - *len, fmt, args);
  va_end(args);

  if (written < 0 || (size_t)written >= request_size - *len)
    return 0;

  *len += (size_t)written;
  return 1;
}

static int build_request(char *request, size_t request_size, size_t *len,
                         const ParsedUrl *url, const char *method,
                         Header *headers, int header_count, const char *body,
                         int keep_alive) {
  *len = 0;
  if (!append_request(request, request_size, len,
                      "%s %s HTTP/1.1\r\nHost: %s\r\nConnection: "
                      "%s\r\nUser-Agent: Mach/1.1\r\n",
                      method, url->path, url->host,
                      keep_alive ? "keep-alive" : "close")) {
    return 0;
  }

  for (int i = 0; i < header_count; i++) {
    if (!append_request(request, request_size, len, "%s: %s\r\n",
                        headers[i].key, headers[i].value)) {
      return 0;
    }
  }

  if (body && body[0] != '\0') {
    if (!append_request(request, request_size, len, "Content-Length: %zu\r\n",
                        strlen(body))) {
      return 0;
    }
  }

  if (!append_request(request, request_size, len, "\r\n"))
    return 0;

  if (body && body[0] != '\0') {
    if (!append_request(request, request_size, len, "%s", body))
      return 0;
  }

  return 1;
}

static int connection_write_all(Connection *conn, const char *data,
                                size_t len) {
  size_t sent = 0;
  while (sent < len) {
    int written;
    if (conn->is_https) {
      written = SSL_write(conn->ssl, data + sent, (int)(len - sent));
    } else {
      written = (int)write(conn->socket, data + sent, len - sent);
    }
    if (written <= 0)
      return 0;
    sent += (size_t)written;
  }
  return 1;
}

void http_init_openssl() {
  SSL_load_error_strings();
  SSL_library_init();
  OpenSSL_add_all_algorithms();
}

void http_cleanup_openssl() { EVP_cleanup(); }

static SSL_CTX *create_ssl_context(int insecure) {
  const SSL_METHOD *method = TLS_client_method();
  SSL_CTX *ctx = SSL_CTX_new(method);
  if (!ctx) {
    return NULL;
  }
  if (insecure) {
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
  }
  return ctx;
}

Connection *http_connect(const char *url_str, int insecure,
                         struct timespec timeout) {
  ParsedUrl parsed;
  if (!url_parse(url_str, &parsed))
    return NULL;

  struct addrinfo hints = {0};
  struct addrinfo *result = NULL;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", parsed.port);

  if (getaddrinfo(parsed.host, port_str, &hints, &result) != 0)
    return NULL;

  int sockfd =
      socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (sockfd < 0) {
    freeaddrinfo(result);
    return NULL;
  }

  struct timeval tv;
  tv.tv_sec = timeout.tv_sec;
  tv.tv_usec = timeout.tv_nsec / 1000;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
  setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));

  if (connect(sockfd, result->ai_addr, result->ai_addrlen) < 0) {
    close(sockfd);
    freeaddrinfo(result);
    return NULL;
  }
  freeaddrinfo(result);

  Connection *conn = malloc(sizeof(Connection));
  if (!conn) {
    close(sockfd);
    return NULL;
  }
  conn->socket = sockfd;
  conn->is_https = parsed.is_https;
  conn->ssl = NULL;
  conn->ctx = NULL;

  if (parsed.is_https) {
    conn->ctx = create_ssl_context(insecure);
    if (!conn->ctx) {
      http_close(conn);
      return NULL;
    }
    conn->ssl = SSL_new(conn->ctx);
    if (!conn->ssl) {
      http_close(conn);
      return NULL;
    }
    SSL_set_tlsext_host_name(conn->ssl, parsed.host);
    SSL_set_fd(conn->ssl, sockfd);
    if (SSL_connect(conn->ssl) <= 0) {
      http_close(conn);
      return NULL;
    }
  }

  return conn;
}

void http_close(Connection *conn) {
  if (!conn)
    return;
  if (conn->ssl) {
    SSL_shutdown(conn->ssl);
    SSL_free(conn->ssl);
  }
  if (conn->ctx)
    SSL_CTX_free(conn->ctx);
  close(conn->socket);
  free(conn);
}

Result http_send(Connection *conn, const char *url_str, const char *method,
                 Header *headers, int header_count, const char *body) {
  Result res = {.url = (char *)url_str,
                .duration_ms = 0,
                .status_code = 0,
                .error = NULL};

  ParsedUrl parsed;
  if (!url_parse(url_str, &parsed)) {
    res.error = "Invalid URL";
    return res;
  }

  char request[8192];
  size_t len = 0;
  if (!build_request(request, sizeof(request), &len, &parsed, method, headers,
                     header_count, body, 1)) {
    res.error = "Request is too large";
    return res;
  }

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  if (!connection_write_all(conn, request, len)) {
    clock_gettime(CLOCK_MONOTONIC, &end);
    res.duration_ms =
        fast_duration_ms(end.tv_sec - start.tv_sec, end.tv_nsec - start.tv_nsec);
    res.error = "Write failed";
    return res;
  }

  char response[4096];
  int bytes_read;
  if (conn->is_https) {
    bytes_read = SSL_read(conn->ssl, response, sizeof(response) - 1);
  } else {
    bytes_read = read(conn->socket, response, sizeof(response) - 1);
  }

  clock_gettime(CLOCK_MONOTONIC, &end);

  // Use ASM-optimized duration calculation
  double duration =
      fast_duration_ms(end.tv_sec - start.tv_sec, end.tv_nsec - start.tv_nsec);

  res.duration_ms = duration;

  if (bytes_read > 0) {
    response[bytes_read] = '\0';
    // Use ASM-optimized status parsing
    res.status_code = fast_parse_status(response);
  } else {
    res.error = "Empty response or read error";
  }

  return res;
}

char *http_fetch_body(const char *url, int insecure) {
  struct timespec timeout = {5, 0};
  Connection *conn = http_connect(url, insecure, timeout);
  if (!conn)
    return NULL;

  ParsedUrl parsed;
  if (!url_parse(url, &parsed)) {
    http_close(conn);
    return NULL;
  }

  char request[1024];
  size_t len = 0;
  if (!build_request(request, sizeof(request), &len, &parsed, "GET", NULL, 0,
                     NULL, 0) ||
      !connection_write_all(conn, request, len)) {
    http_close(conn);
    return NULL;
  }

  char *buffer = malloc(65536);
  if (!buffer) {
    http_close(conn);
    return NULL;
  }
  int total_read = 0;
  int bytes_read;
  while (1) {
    if (conn->is_https) {
      bytes_read =
          SSL_read(conn->ssl, buffer + total_read, 65536 - total_read - 1);
    } else {
      bytes_read =
          read(conn->socket, buffer + total_read, 65536 - total_read - 1);
    }
    if (bytes_read <= 0)
      break;
    total_read += bytes_read;
    if (total_read >= 65535)
      break;
  }
  buffer[total_read] = '\0';
  http_close(conn);

  // Skip headers to find body
  char *body = strstr(buffer, "\r\n\r\n");
  if (body) {
    char *result = strdup(body + 4);
    free(buffer);
    return result;
  }

  free(buffer);
  return NULL;
}

int http_download_to_file(const char *url, const char *path_to_save,
                          int insecure) {
  struct timespec timeout = {30, 0};
  Connection *conn = http_connect(url, insecure, timeout);
  if (!conn)
    return -1;

  ParsedUrl parsed;
  if (!url_parse(url, &parsed)) {
    http_close(conn);
    return -1;
  }

  char request[1024];
  size_t len = 0;
  if (!build_request(request, sizeof(request), &len, &parsed, "GET", NULL, 0,
                     NULL, 0) ||
      !connection_write_all(conn, request, len)) {
    http_close(conn);
    return -1;
  }

  FILE *fp = fopen(path_to_save, "wb");
  if (!fp) {
    http_close(conn);
    return -1;
  }

  char buffer[8192];
  int bytes_read;
  int header_skipped = 0;
  while (1) {
    if (conn->is_https) {
      bytes_read = SSL_read(conn->ssl, buffer, sizeof(buffer));
    } else {
      bytes_read = read(conn->socket, buffer, sizeof(buffer));
    }
    if (bytes_read <= 0)
      break;

    if (!header_skipped) {
      char *body = strstr(buffer, "\r\n\r\n");
      if (body) {
        int header_len = (body + 4) - buffer;
        fwrite(body + 4, 1, bytes_read - header_len, fp);
        header_skipped = 1;
      }
    } else {
      fwrite(buffer, 1, bytes_read, fp);
    }
  }

  fclose(fp);
  http_close(conn);
  return 0;
}
