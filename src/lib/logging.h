#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h> // 為了 va_list

/* (要求 4) 日誌系統 */
extern int g_debug_level;
void setup_logging();
void log_msg(int level, const char *format, ...);

#endif // LOGGING_H