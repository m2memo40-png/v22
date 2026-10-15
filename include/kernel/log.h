#ifndef FLUXMAX_LOG_H
#define FLUXMAX_LOG_H
#include <stdint.h>
typedef enum { LOG_TRACE=0, LOG_DEBUG, LOG_INFO, LOG_WARN, LOG_ERROR, LOG_FATAL } log_level_t;
void serial_init(void);
void serial_write_char(char c);
void serial_write(const char *s);
void debugcon_write(const char *s);
int serial_try_read_char(char *out);
void serial_write_hex(uint64_t value);
void log_set_level(log_level_t level);
void log_write(log_level_t level, const char *component, const char *message);
#endif
