#include "http.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>

// ASM-optimized functions
extern int fast_parse_status(char *response);
extern double fast_duration_ms(long sec_diff, long nsec_diff);

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
    perror("Unable to create SSL context");
    exit(EXIT_FAILURE);
  }
  if (insecure) {
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
  }
  return ctx;
}

Connection *http_connect(const char *url_str, int insecure,
                         struct timespec timeout) {
  char protocol[8], host[256], path[1024];
  int port = 80;
  int is_https = 0;

  if (sscanf(url_str, "%7[^ : ]://%255[^:/]:%d%1023s", protocol, host, &port,
             path) == 4) {
  } else if (sscanf(url_str, "%7[^ : ]://%255[^:/]%1023s", protocol, host,
                    path) == 3) {
  } else if (sscanf(url_str, "%7[^ : ]://%255[^:/]", protocol, host) == 2) {
    strcpy(path, "/");
  } else {
    return NULL;
  }

  if (strcmp(protocol, "https") == 0) {
    is_https = 1;
    if (port == 80)
      port = 443;
  }

  struct hostent *server = gethostbyname(host);
  if (server == NULL)
    return NULL;

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    return NULL;

  struct sockaddr_in serv_addr;
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  serv_addr.sin_port = htons(port);

  struct timeval tv;
  tv.tv_sec = timeout.tv_sec;
  tv.tv_usec = timeout.tv_nsec / 1000;
  setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
  setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));

  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    close(sockfd);
    return NULL;
  }

  Connection *conn = malloc(sizeof(Connection));
  conn->socket = sockfd;
  conn->is_https = is_https;
  conn->ssl = NULL;
  conn->ctx = NULL;

  if (is_https) {
    conn->ctx = create_ssl_context(insecure);
    conn->ssl = SSL_new(conn->ctx);
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
  char host[256], path[1024];
  if (sscanf(url_str, "%*[^ : ]://%255[^:/]:%*d%1023s", host, path) != 2) {
    if (sscanf(url_str, "%*[^ : ]://%255[^:/]%1023s", host, path) != 2) {
      if (sscanf(url_str, "%*[^ : ]://%255[^:/]", host) == 1) {
        strcpy(path, "/");
      }
    }
  }

  char request[4096];
  int len = snprintf(request, sizeof(request),
                     "%s %s HTTP/1.1\r\nHost: %s\r\nConnection: "
                     "keep-alive\r\nUser-Agent: Mach/1.0\r\n",
                     method, path, host);

  for (int i = 0; i < header_count; i++) {
    len += snprintf(request + len, sizeof(request) - len, "%s: %s\r\n",
                    headers[i].key, headers[i].value);
  }

  if (body && strlen(body) > 0) {
    len += snprintf(request + len, sizeof(request) - len,
                    "Content-Length: %zu\r\n", strlen(body));
  }
  len += snprintf(request + len, sizeof(request) - len, "\r\n");

  if (body && strlen(body) > 0) {
    len += snprintf(request + len, sizeof(request) - len, "%s", body);
  }

  struct timespec start, end;
  clock_gettime(CLOCK_MONOTONIC, &start);

  if (conn->is_https) {
    SSL_write(conn->ssl, request, len);
  } else {
    write(conn->socket, request, len);
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

  Result res = {.url = (char *)url_str,
                .duration_ms = duration,
                .status_code = 0,
                .error = NULL};

  if (bytes_read > 0) {
    response[bytes_read] = '\0';
    // Use ASM-optimized status parsing
    res.status_code = fast_parse_status(response);
  } else {
    res.error = "Empty response or read error";
  }

  return res;
}
