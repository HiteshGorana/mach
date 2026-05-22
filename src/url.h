#ifndef URL_H
#define URL_H

typedef struct {
  char protocol[8];
  char host[256];
  char path[1024];
  int port;
  int is_https;
} ParsedUrl;

int url_parse(const char *url, ParsedUrl *out);
int url_is_supported(const char *url);

#endif
