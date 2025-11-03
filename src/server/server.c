#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include "logging.h"     // 引入日誌系統
#include "robust_io.h"   // 引入強健 I/O

#define PORT 9999
#define MAX_BUFFER 8192 // HTTP 標頭可能很長

bool g_maintenance_mode = false;

// --- 你的 sigchld_handler 保持不變 ---
void sigchld_handler(int sig) {
    (void)sig;
    int saved_errno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

// --- 你的 sigusr1_handler 保持不變 ---
void sigusr1_handler(int sig) {
    (void)sig;
    g_maintenance_mode = !g_maintenance_mode;
    log_msg(1, "MAINTENANCE MODE TOGGLED TO: %s\n", g_maintenance_mode ? "ON" : "OFF");
}

/**
 * 讀取 /proc/loadavg 並組合回應
 */
void get_load_avg_body(char *body_buffer, size_t max_len) {
    FILE *fp = fopen("/proc/loadavg", "r");
    if (fp == NULL) {
        snprintf(body_buffer, max_len, "Error: Could not open /proc/loadavg: %s", strerror(errno));
        return;
    }
    
    float load1, load5, load15;
    // 讀取檔案中的前三個浮點數
    if (fscanf(fp, "%f %f %f", &load1, &load5, &load15) == 3) {
        snprintf(body_buffer, max_len,
                 "Server Load Average:\n"
                 " 1 min: %.2f\n"
                 " 5 min: %.2f\n"
                 "15 min: %.2f\n",
                 load1, load5, load15);
    } else {
        strcpy(body_buffer, "Error: Could not parse /proc/loadavg");
    }
    fclose(fp);
}

/**
 * 傳送一個完整的 HTTP 回應
 */
void send_http_response(int connfd, const char *status, const char *content_type, const char *body) {
    char http_header[MAX_BUFFER];
    int body_len = strlen(body);

    // 1. 組合 HTTP 標頭
    snprintf(http_header, MAX_BUFFER,
             "HTTP/1.1 %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n" // 告知客戶端我們會關閉連線
             "\r\n",
             status, content_type, body_len);

    // 2. 使用你的 writen 函式傳送標頭
    writen(connfd, http_header, strlen(http_header));
    // 3. 使用你的 writen 函式傳送內容 (Body)
    writen(connfd, (char*)body, body_len);
}

/**
 * 讀取 /proc/meminfo 並組合回應 (你的舊功能)
 */
void get_ram_info_body(char *body_buffer, size_t max_len) {
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp == NULL) {
        snprintf(body_buffer, max_len, "Error: Could not open /proc/meminfo: %s", strerror(errno));
        return;
    }
    char line_buffer[256];
    body_buffer[0] = '\0'; // 清空 buffer
    while (fgets(line_buffer, sizeof(line_buffer), fp) != NULL) {
        if (strncmp(line_buffer, "MemTotal:", 9) == 0) {
            strcat(body_buffer, line_buffer);
        } else if (strncmp(line_buffer, "MemFree:", 8) == 0) {
            strcat(body_buffer, line_buffer);
            break;
        }
    }
    fclose(fp);
}

/**
 * 執行 'date' 命令並組合回應
 */
/**
 * 執行 'date' 命令並組合回應 (已指定時區)
 */
void get_time_body(char *body_buffer, size_t max_len) {
    // TZ='Asia/Taipei'：強制 date 指令使用台北時區
    // +'%Y-%m-%d %H:%M:%S (Asia/Taipei)'：自訂輸出格式
    const char *date_cmd = "TZ='Asia/Taipei' date +'%Y-%m-%d %H:%M:%S (Asia/Taipei)'";

    FILE *fp = popen(date_cmd, "r");
    if (fp == NULL) {
        snprintf(body_buffer, max_len, "Error: popen(date) failed");
        return;
    }
    if (fgets(body_buffer, max_len - 1, fp) == NULL) {
        strcpy(body_buffer, "Error: fgets(date) failed");
    }
    // 確保移除 fgets 可能讀到的換行符
    body_buffer[strcspn(body_buffer, "\n")] = 0;
    pclose(fp);
}

/**
 * 執行 'df -h /' 並組合回應 (展示如何處理多行)
 */
void get_disk_usage_body(char *body_buffer, size_t max_len) {
    // 執行 'df -h' 並只看根目錄 '/'
    FILE *fp = popen("df -h /", "r");
    if (fp == NULL) {
        snprintf(body_buffer, max_len, "Error: popen(df) failed");
        return;
    }
    
    char line_buffer[256];
    body_buffer[0] = '\0'; // 清空
    size_t total_len = 0;

    // 讀取 'df -h' 的所有輸出 (通常是 2 行)
    while (fgets(line_buffer, sizeof(line_buffer), fp) != NULL) {
        size_t line_len = strlen(line_buffer);
        if (total_len + line_len < max_len) {
            strcat(body_buffer, line_buffer);
            total_len += line_len;
        } else {
            break; // 避免緩衝區溢位
        }
    }
    pclose(fp);
}

/**
 * 讀取 uname -a 並組合回應 (你的舊功能)
 */
void get_sys_info_body(char *body_buffer, size_t max_len) {
    FILE *fp = popen("uname -a", "r");
    if (fp == NULL) {
        snprintf(body_buffer, max_len, "Error: popen() failed");
        return;
    }
    if (fgets(body_buffer, max_len - 1, fp) == NULL) {
        strcpy(body_buffer, "Error: fgets() failed");
    }
    pclose(fp);
}

/**
 * 處理 HTTP 連線 (已升級)
 * 替換了你舊的 handle_connection
 */
void handle_http_connection(int connfd, struct sockaddr_in client_addr) {
    char buf[MAX_BUFFER];
    char method[MAX_BUFFER], uri[MAX_BUFFER], version[MAX_BUFFER];
    char client_ip[INET_ADDRSTRLEN];
    
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    int client_port = ntohs(client_addr.sin_port);
    pid_t pid = getpid();
    
    log_msg(1, "PID %d: Handling connection from %s:%d\n", pid, client_ip, client_port);

    // 1. 使用 readline_line 讀取 HTTP 請求的第一行
    if (readline_line(connfd, buf, MAX_BUFFER) <= 0) {
        log_msg(0, "PID %d: readline_line error or client disconnected.\n", pid);
        return;
    }
    
    // 2. 解析請求行 (例如 "GET /raminfo HTTP/1.1")
    sscanf(buf, "%s %s %s", method, uri, version);
    log_msg(2, "PID %d: Received Request: %s %s %s\n", pid, method, uri, version);

    // 3. (可選) 讀取並丟棄所有剩餘的 HTTP 標頭，直到空行
    while(readline_line(connfd, buf, MAX_BUFFER) > 0) {
        if (strcmp(buf, "\r\n") == 0) {
            break; // 標頭結束
        }
    }

    // 4. 根據 URI 路由 (Routing)
    char response_body[MAX_BUFFER];
    
    if (strcmp(uri, "/raminfo") == 0) {
        // --- 呼叫你的舊功能 ---
        get_ram_info_body(response_body, MAX_BUFFER);
        send_http_response(connfd, "200 OK", "text/plain", response_body);
        log_msg(1, "PID %d: Sent RAM info to %s:%d\n", pid, client_ip, client_port);

    } else if (strcmp(uri, "/sysinfo") == 0) {
        // --- 呼叫你的舊功能 ---
        get_sys_info_body(response_body, MAX_BUFFER);
        send_http_response(connfd, "200 OK", "text/plain", response_body);
        log_msg(1, "PID %d: Sent SYS info to %s:%d\n", pid, client_ip, client_port);

    } else if (strcmp(uri, "/loadavg") == 0) { 
        get_load_avg_body(response_body, MAX_BUFFER);
        send_http_response(connfd, "200 OK", "text/plain", response_body);
        log_msg(1, "PID %d: Sent LOADAVG info to %s:%d\n", pid, client_ip, client_port);

    } else if (strcmp(uri, "/time") == 0) { 
        get_time_body(response_body, MAX_BUFFER);
        send_http_response(connfd, "200 OK", "text/plain", response_body);
        log_msg(1, "PID %d: Sent TIME info to %s:%d\n", pid, client_ip, client_port);

    } else if (strcmp(uri, "/diskusage") == 0) { 
        get_disk_usage_body(response_body, MAX_BUFFER);
        send_http_response(connfd, "200 OK", "text/plain", response_body);
        log_msg(1, "PID %d: Sent DISKUSAGE info to %s:%d\n", pid, client_ip, client_port);

    } else {
        // 404 Not Found
        snprintf(response_body, MAX_BUFFER, "404 Not Found: URI '%s' is not recognized. Try /raminfo or /sysinfo.", uri);
        send_http_response(connfd, "404 Not Found", "text/plain", response_body);
        log_msg(1, "PID %d: Sent 404 for URI %s\n", pid, uri);
    }
}


/**
 * 你的 main 函式 (保持了所有的健全性機制)
 */
int main() {
    int listenfd, connfd;
    struct sockaddr_in servaddr, client_addr;
    socklen_t client_len;
    pid_t child_pid;

    setup_logging();

    // --- 健全性 1：註冊 SIGCHLD handler ---
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, 0) == -1) {
        log_msg(0, "sigaction(SIGCHLD) failed: %s\n", strerror(errno));
        exit(1);
    }

    // 註冊 SIGUSR1 (你的 Redirect 功能)
    sa.sa_handler = sigusr1_handler;
    if (sigaction(SIGUSR1, &sa, 0) == -1) {
        log_msg(0, "sigaction(SIGUSR1) failed: %s\n", strerror(errno));
        exit(1);
    }
    
    // --- 健全性 2：忽略 SIGPIPE 訊號 ---
    signal(SIGPIPE, SIG_IGN);

    signal(SIGTSTP, SIG_IGN);

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log_msg(0, "socket error: %s\n", strerror(errno));
        exit(1);
    }

    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(PORT);

    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        log_msg(0, "bind error: %s\n", strerror(errno));
        exit(1);
    }

    if (listen(listenfd, 1024) < 0) {
        log_msg(0, "listen error: %s\n", strerror(errno));
        exit(1);
    }

    log_msg(1, "Server started on http://127.0.0.1:%d, PID %d. Waiting for connections...\n", PORT, getpid());

    while (1) {
        client_len = sizeof(client_addr);
        connfd = accept(listenfd, (struct sockaddr *)&client_addr, &client_len);

        if (connfd < 0) {
            // --- 健全性 3：處理被中斷的系統呼叫 ---
            if (errno == EINTR) {
                log_msg(2, "accept() was interrupted by a signal, retrying...\n");
                continue;
            } else {
                log_msg(0, "accept error: %s\n", strerror(errno));
                continue; 
            }
        }
        
        // --- 健全性 4：Redirect (維護模式) ---
        if (g_maintenance_mode) {
            log_msg(1, "Server in maintenance. Redirecting client.\n");
            // 回應一個合法的 HTTP 503 錯誤
            const char *body = "503 Service Unavailable: Server is in maintenance mode.";
            send_http_response(connfd, "503 Service Unavailable", "text/plain", body);
            close(connfd);
            continue;
        }

        // --- 你的 `fork()` 模型保持不變 ---
        if ((child_pid = fork()) < 0) {
            log_msg(0, "fork error: %s\n", strerror(errno));
            close(connfd);
        } 
        else if (child_pid == 0) {
            // --- 子程序 ---
            log_msg(2, "Child process %d created.\n", getpid());
            close(listenfd);
            handle_http_connection(connfd, client_addr); // 呼叫新的 HTTP handler
            close(connfd);
            log_msg(2, "Child process %d finished.\n", getpid());
            exit(0);
        } 
        else {
            // --- 父程序 ---
            close(connfd); 
        }
    }
    return 0;
}