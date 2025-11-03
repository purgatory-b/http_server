#ifndef ROBUST_IO_H
#define ROBUST_IO_H

#include <sys/types.h>
#include <unistd.h> // 為了 ssize_t

/* (要求 7) 強健的 I/O 函式 */

/**
 * 一次讀取一行 (line-based) 的強健 I/O
 */
ssize_t readline_line(int fd, char* buffer, size_t max_len);

/**
 * 確保寫入 n 個位元組
 */
ssize_t writen(int fd, const void *vptr, size_t n);

#endif // ROBUST_IO_H