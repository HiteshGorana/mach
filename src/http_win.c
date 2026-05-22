// http_win.c - Windows Winsock implementation
#ifdef _WIN32

#include "http.h"
#include "url.h"
#include <stdarg.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

static WSADATA wsaData;
static int wsa_initialized = 0;

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
      written = send((SOCKET)conn->socket, data + sent, (int)(len - sent), 0);
    }
    if (written <= 0)
      return 0;
    sent += (size_t)written;
  }
  return 1;
}

void http_init_openssl() {
  if (!wsa_initialized) {
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    wsa_initialized = 1;
  }
  SSL_load_error_strings();
  SSL_library_init();
  OpenSSL_add_all_algorithms();
}

void http_cleanup_openssl() {
  EVP_cleanup();
  if (wsa_initialized) {
    WSACleanup();
    wsa_initialized = 0;
  }
}

Connection *http_connect(const char *url_str, int insecure,
                         struct timespec timeout) {
  ParsedUrl parsed;
  if (!url_parse(url_str, &parsed))
    return NULL;

  struct addrinfo hints = {0}, *result;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;

  char port_str[16];
  snprintf(port_str, sizeof(port_str), "%d", parsed.port);

  if (getaddrinfo(parsed.host, port_str, &hints, &result) != 0) {
    return NULL;
  }

  SOCKET sockfd =
      socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (sockfd == INVALID_SOCKET) {
    freeaddrinfo(result);
    return NULL;
  }

  // Set timeout
  DWORD tv = (DWORD)(timeout.tv_sec * 1000 + timeout.tv_nsec / 1000000);
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
  setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));

  if (connect(sockfd, result->ai_addr, (int)result->ai_addrlen) ==
      SOCKET_ERROR) {
    closesocket(sockfd);
    freeaddrinfo(result);
    return NULL;
  }
  freeaddrinfo(result);

  Connection *conn = malloc(sizeof(Connection));
  if (!conn) {
    closesocket(sockfd);
    return NULL;
  }
  conn->socket = (int)sockfd;
  conn->is_https = parsed.is_https;
  conn->ssl = NULL;
  conn->ctx = NULL;

  if (parsed.is_https) {
    const SSL_METHOD *method = TLS_client_method();
    conn->ctx = SSL_CTX_new(method);
    if (!conn->ctx) {
      http_close(conn);
      return NULL;
    }
    if (insecure) {
      SSL_CTX_set_verify(conn->ctx, SSL_VERIFY_NONE, NULL);
    }
    conn->ssl = SSL_new(conn->ctx);
    if (!conn->ssl) {
      http_close(conn);
      return NULL;
    }
    SSL_set_tlsext_host_name(conn->ssl, parsed.host);
    SSL_set_fd(conn->ssl, (int)sockfd);
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
  closesocket((SOCKET)conn->socket);
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

  LARGE_INTEGER freq, start, end;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&start);

  if (!connection_write_all(conn, request, len)) {
    QueryPerformanceCounter(&end);
    res.duration_ms =
        ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart) * 1000.0;
    res.error = "Write failed";
    return res;
  }

  char response[4096];
  int bytes_read;
  if (conn->is_https) {
    bytes_read = SSL_read(conn->ssl, response, sizeof(response) - 1);
  } else {
    bytes_read = recv((SOCKET)conn->socket, response, sizeof(response) - 1, 0);
  }

  QueryPerformanceCounter(&end);
  double duration =
      ((double)(end.QuadPart - start.QuadPart) / freq.QuadPart) * 1000.0;

  res.duration_ms = duration;

  if (bytes_read > 0) {
    response[bytes_read] = '\0';
    extern int fast_parse_status(char *);
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
      bytes_read = recv((SOCKET)conn->socket, buffer + total_read,
                        65536 - total_read - 1, 0);
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
    char *result = _strdup(body + 4);
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
      bytes_read = recv((SOCKET)conn->socket, buffer, sizeof(buffer), 0);
    }
    if (bytes_read <= 0)
      break;

    if (!header_skipped) {
      char *body = strstr(buffer, "\r\n\r\n");
      if (body) {
        int header_len = (int)((body + 4) - buffer);
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

#endif // _WIN32
