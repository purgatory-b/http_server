#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>      // 為了 errno
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "logging.h"     // 引入日誌系統
#include "robust_io.h"   // 引入強健 I/O

#define PORT 9999
#define SERVER_IP "127.0.0.1"
#define MAX_BUFFER 8192

// --- 移除舊的 #define URI ---
// #define URI "/raminfo"

// 讓 main 函式接收命令列參數
int main(int argc, char *argv[]) {
    int sockfd;
    struct sockaddr_in servaddr;
    char request_buf[MAX_BUFFER];
    char response_buf[MAX_BUFFER];
    
    char *uri_to_request; // 用一個變數來儲存要請求的 URI

    setup_logging();

    // --- ★★★ 這是新的動態 URI 邏輯 ★★★ ---
    if (argc == 2) {
        // 如果使用者提供了 1 個參數 (e.g., ./client /sysinfo)
        // argv[0] 是程式名稱 (./client)
        // argv[1] 是第一個參數 ("/sysinfo")
        uri_to_request = argv[1];
    } else {
        // 如果使用者沒有提供參數，我們就使用一個預設值
        uri_to_request = "/raminfo";
        log_msg(1, "Hint: No URI provided. Defaulting to /raminfo\n");
        log_msg(1, "Hint: Try running: ./client /sysinfo\n");
    }
    // --- ★★★ 邏輯結束 ★★★ ---


    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log_msg(0, "socket error: %s\n", strerror(errno));
        exit(1);
    }

    // ... (中略，bind, connect 的程式碼都一樣) ...
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER_IP, &servaddr.sin_addr) <= 0) {
        log_msg(0, "inet_pton error for %s\n", SERVER_IP);
        exit(1);
    }
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        log_msg(0, "connect error: %s\n", strerror(errno));
        exit(1);
    }
    log_msg(1, "Connected to server %s:%d\n", SERVER_IP, PORT);


    // 4. --- 建立一個 HTTP 1.1 請求 ---
    // (注意：這裡的 URI 換成了我們的動態變數 uri_to_request)
    snprintf(request_buf, MAX_BUFFER,
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n"
             "\r\n", // 空行代表標頭結束
             uri_to_request, SERVER_IP); // ★★★ 使用變數 ★★★
             
    log_msg(2, "Sending request for URI: %s\n", uri_to_request);
    
    // 5. --- 傳送請求 (使用你的 writen 函式) ---
    if (writen(sockfd, request_buf, strlen(request_buf)) < 0) {
        log_msg(0, "writen error: %s\n", strerror(errno));
        close(sockfd);
        exit(1);
    }

    // 6. --- 讀取回應 (使用你的 readline_line 函式) ---
    // ... (讀取回應的 while 迴圈保持不變) ...
    printf("\n--- Server Response ---\n");
    ssize_t n;
    while ((n = readline_line(sockfd, response_buf, MAX_BUFFER)) > 0) {
        printf("%s", response_buf);
    }
    printf("-------------------------\n");

    close(sockfd);
    log_msg(1, "Connection closed.\n");
    return 0;
}