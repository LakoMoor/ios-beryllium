#ifndef CRUMB_H
#define CRUMB_H
#include <stdint.h>
void crumb_init(void);
void crumb_str(const char *s);
void crumb_hex(uint64_t v);
void crumb_line(const char *label, const char *s, uint64_t v);
void crumb_start(uint64_t x0);
#endif
