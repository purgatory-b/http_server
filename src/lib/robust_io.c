#include "robust_io.h"
#include "logging.h" // 為了 log_msg
#include <errno.h>   // 為了 EINTR
#include <string.h>  // 為了 strerror

/* ==================================================================
 * (要求 7) 強健的 I/O 函式實作
 * ================================================================== */

ssize_t readline_line(int fd, char* buffer, size_t max_len)
{
    int result;
    char c;
    size_t read_bytes = 0;

    // -1 是為了保留空間給 '\0'
    for(read_bytes = 0 ; read_bytes < max_len - 1 ; read_bytes++){
        result = read(fd, &c, 1);

        if(result == 1){
            *buffer = c;
            buffer++;
            
            if(c == '\n'){
                read_bytes++; // 包含 \n
                break;
            }

        } else if(result == 0){ // EOF
            if(read_bytes > 0){
                break; // 讀到一半，對端關閉
            } else {
                return 0; // 沒讀到任何東西
            }
        } else { // 錯誤
            if (errno == EINTR) // 處理被中斷的系統呼叫
                continue;
            return -1; // 真正發生錯誤
        }
    }

    *buffer = '\0'; // 加上結尾
    return read_bytes;
}

ssize_t writen(int fd, const void *vptr, size_t n) {
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0; // 被訊號中斷，重新呼叫 write
            else {
                // 使用日誌系統回報錯誤
                log_msg(0, "writen error: %s", strerror(errno));
                return -1; // 發生錯誤 (例如 EPIPE)
            }
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return n;
}