#include "logging.h"

/* ==================================================================
 * (要求 4) 日誌系統實作
 * ================================================================== */

int g_debug_level = 0;

void setup_logging() {
    char *env_dbg = getenv("MY_APP_DEBUG");
    if (env_dbg) {
        g_debug_level = atoi(env_dbg);
        if (g_debug_level < 0) g_debug_level = 0;
        if (g_debug_level > 2) g_debug_level = 2;
    }
}

void log_msg(int level, const char *format, ...) {
#ifndef DEBUG_BUILD
    if (level > 0) {
        return;
    }
#endif
    if (level > g_debug_level) {
        return;
    }
    va_list args;
    va_start(args, format);
    switch (level) {
        case 0: fprintf(stderr, "[ERROR] "); break;
        case 1: fprintf(stdout, "[INFO]  "); break;
        case 2: fprintf(stdout, "[DEBUG] "); break;
    }
    vfprintf(level == 0 ? stderr : stdout, format, args);
    va_end(args);
}