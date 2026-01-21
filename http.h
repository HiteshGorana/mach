#ifndef HTTP_H
#define HTTP_H

#include "blitz.h"

typedef struct {
  int socket;
  SSL *ssl;
  SSL_CTX *ctx;
  int is_https;
} Connection;

void http_init_openssl();
void http_cleanup_openssl();

Connection *http_connect(const char *url, int insecure,
                         struct timespec timeout);
void http_close(Connection *conn);
Result http_send(Connection *conn, const char *url, const char *method,
                 Header *headers, int header_count, const char *body);

#endif
