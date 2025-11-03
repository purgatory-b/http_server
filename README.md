# 穩健的 C 語言 HTTP 伺服器 (Robust C HTTP Server)

## 1\. 應用程式介紹 (Application Introduction)

本專案是一個使用 C 語言編寫的、高效能且具備高健全性 (Robustness) 的並行 (Concurrent) HTTP 1.1 伺服器。

伺服器採用 `fork()` (process-per-client) 的並行模型，確保每個客戶端連線都在一個獨立的行程中處理，實現了完美的錯誤隔離（一個連線的崩潰不會影響主服務）。

本專案採用 `CMake` 進行建構管理，並遵循「來源外建置」(out-of-source build) 的最佳實踐。所有核心共享功能（如日誌 和強健 I/O）均被封裝在一個動態共享函式庫 (`libhttp_lib.so`) 中。

伺服器 提供了一個基於 HTTP 協議的 API，允許客戶端 動態查詢多種即時的系統資訊。

## 2\. 專案特色 (Features)

  * **HTTP 1.1 API 伺服器**：
    伺服器 實作了一個可擴充的 API，能解析 HTTP `GET` 請求，並依據 URI 進行路由：

      * `GET /raminfo`：回傳伺服器記憶體資訊 (讀取 `/proc/meminfo`)。
      * `GET /sysinfo`：回傳 `uname -a` 的系統資訊。
      * `GET /loadavg`：回傳 `/proc/loadavg` 的系統負載。
      * `GET /time`：回傳 `Asia/Taipei` 的目前時間。
      * `GET /diskusage`：回傳 `df -h /` 的磁碟使用狀況。
      * `GET /crashme`：一個用於測試健全性的 API，會觸發 `SIGSEGV`。

  * **`fork()` 並行模型**：
    伺服器 在 `accept()` 之後，會立即呼叫 `fork()` 建立一個子程序來處理單一客戶端連線。

  * **動態共享函式庫 (`.so`)**：
    所有共享邏輯都被拆分並封裝在 `src/lib/` 目錄下（包含 `logging.c` 和 `robust_io.c`）。`CMake` 會將它們編譯為 `libhttp_lib.so`，並由 `server` 和 `client` 連結。

  * **兩階段日誌控制 (Two-Stage Logging)**：

    1.  **編譯時期**：`CMakeLists.txt` 會偵測 `CMAKE_BUILD_TYPE=Debug`，並定義 `DEBUG_BUILD` 宏。在 `Release` 模式下，`[INFO]` 和 `[DEBUG]` 日誌會被編譯器移除。
    2.  **執行時期**：`logging.c` 會讀取 `MY_APP_DEBUG` 環境變數，來決定是否印出日誌。

  * **健全性機制 (Robustness)**：
    伺服器 實作了多種訊號處理機制 來處理異常狀況：

    1.  **`sigaction(SIGCHLD, ...)`**：捕捉 `SIGCHLD` 訊號並呼叫 `waitpid()`，防止「殭屍程序」(Zombie Process) 產生。
    2.  **`signal(SIGPIPE, SIG_IGN)`**：忽略 `SIGPIPE` 訊號，防止子程序因客戶端突然斷線而崩潰。
    3.  **`if (errno == EINTR)`**：在 `accept()` 之後檢查「被中斷的系統呼叫」，防止主迴圈被訊號干擾。
    4.  **`signal(SIGTSTP, ...)`**：捕捉 `Ctrl+Z` 訊號，並呼叫 `exit()`，使伺服器能乾淨地關閉並釋放埠號。

## 3\. 專案結構 (Project Structure)

本專案將所有原始碼 (`.c`/`.h`) 統一放在 `src/` 目錄下，與建構目錄 (`build/`) 和專案管理檔案完全分離。

```
http_server/ (專案根目錄)
│
├── CMakeLists.txt          # 根 CMake 腳本
├── test_concurrent.sh      # 並行壓力測試腳本
├── README.md               # (本檔案)
├── .gitignore              # (Git 忽略檔案，用來忽略 build/)
│
├── src/                    # (原始碼目錄)
│   │
│   ├── client/
│   │   └── client.c        # 客戶端原始碼
│   │
│   ├── server/
│   │   └── server.c        # 伺服器原始碼
│   │
│   └── lib/                # (共享函式庫原始碼)
│       ├── CMakeLists.txt  # lib 的 CMake 腳本
│       ├── logging.c       # 日誌系統實作
│       ├── logging.h
│       ├── robust_io.c     # I/O 函式實作 (readline_line, writen)
│       └── robust_io.h
│
└── build/                  # (編譯產物目錄 - 由 'cmake' 產生)
    │
    ├── server                  # (★ 最終的伺服器執行檔)
    ├── client                  # (★ 最終的客戶端執行檔)
    │
    ├── Makefile                # (CMake 自動產生的 Makefile)
    ├── CMakeCache.txt          # (CMake 的設定快取)
    │
    └── lib/                    # (CMake 會建立一個鏡像目錄來編譯 lib)
        └── libhttp_lib.so      # (★ 最終的共享函式庫)
```

## 4\. Flow Explanation (流程說明)

本專案的執行流程可以分為「伺服器啟動」和「客戶端請求」兩個階段：

**A. 伺服器啟動流程 (`server`)**

1.  **初始化**：伺服器啟動 `main()`，呼叫 `setup_logging()` 初始化日誌系統。
2.  **註冊訊號**：設定**健全性機制**，包含 `sigaction(SIGCHLD, ...)` (回收殭屍程序)、`signal(SIGPIPE, SIG_IGN)` (忽略斷線) 和 `signal(SIGTSTP, ...)` (處理 `Ctrl+Z`)。
3.  **綁定埠號**：執行 `socket()`, `bind()`, `listen()` 來開啟 9999 埠的監聽。
4.  **等待連線**：主程序進入 `while(1)` 迴圈，阻塞在 `accept()`，等待客戶端連線。

**B. 客戶端請求流程 (`client`)**

1.  **啟動**：客戶端 `main()` 啟動，並解析命令列參數 (`argv[1]`) 來決定要請求的 URI (例如 `/time`)。
2.  **連線**：執行 `socket()` 和 `connect()` 連線到 9999 埠。
3.  **傳送 HTTP 請求**：使用 `snprintf()` 組合一個 HTTP 1.1 `GET` 請求字串，並呼叫共享函式庫中的 `writen()` 將請求傳送出去。
4.  **伺服器 `fork`**：伺服器的 `accept()` 收到連線並返回。主程序**立即呼叫 `fork()`**，建立一個**子程序**。
5.  **伺服器子程序處理**：
      * 子程序呼叫 `handle_http_connection()`。
      * 使用 `readline_line()` 讀取 HTTP 請求行。
      * 透過 `strcmp()` 進行**路由判斷**，找到對應的 `uri` (例如 `/time`)。
      * 呼叫 `get_time_body()`，此函式執行 `popen("date", ...)` 取得結果。
      * 呼叫 `send_http_response()` 組合 `HTTP/1.1 200 OK` 標頭。
      * 使用 `writen()` 將標頭和內容傳回給客戶端。
      * 子程序呼叫 `exit(0)` 結束。
6.  **客戶端接收**：客戶端的 `readline_line()` 迴圈開始接收資料，並將 HTTP 回應（標頭和內容）印在螢幕上。
7.  **伺服器主程序**：
      * 主程序（父程序）在 `fork()` 後，立即回到 `while(1)` 迴圈的頂部，**再次呼叫 `accept()`**，準備服務下一個客戶端。
      * 當子程序 `exit(0)` 時，`SIGCHLD` 訊號被觸發，`sigchld_handler` 會呼叫 `waitpid` 將其回收。

## 5\. 如何編譯與執行 (Real Test)

**需要開啟多個終端機。**

### A. 編譯 (CMake)

請勿在原始碼目錄中編譯。

1.  **建立 `build` 目錄**：

    ```bash
    mkdir build
    cd build
    ```

2.  **執行 CMake (產生建構檔)**：

      * **選項 A：編譯 Debug 版本 (推薦，會印出日誌)**
        ```bash
        # 這會定義 -DDEBUG_BUILD
        cmake -DCMAKE_BUILD_TYPE=Debug ..
        ```
      * **選項 B：編譯 Release 版本 (效能最高，不含日誌)**
        ```bash
        # 這「不會」定義 -DDEBUG_BUILD
        cmake -DCMAKE_BUILD_TYPE=Release ..
        ```

3.  **執行 Make (編譯程式)**：

    ```bash
    make
    ```

    編譯好的 `server` 和 `client` 執行檔會出現在 `build/` 目錄中。

### B. 執行與測試

**1. 啟動伺服器**
(在 `build/` 目錄下)

```bash
# 啟動伺服器，並設定執行時期日誌等級為 1 (INFO)
MY_APP_DEBUG=1 ./server
```

伺服器將啟動並監聽 `http://127.0.0.1:9999`。

**2. 執行客戶端 (動態 URI)**
客戶端 可以接收一個命令列參數作為要請求的 URI。

(在 `build/` 目錄下，開啟新終端機)

```bash
# 測試 RAM 資訊
./client /raminfo

# 測試系統時間
./client /time

# 測試 404
./client /badpath
```

**3. 使用 `curl` 測試**
`curl` 是測試 HTTP 伺服器最快的方式。

```bash
# 測試系統負載
curl http://127.0.0.1:9999/loadavg

# 測試磁碟使用率
curl http://127.0.0.1:9999/diskusage
```

**4. 並行測試 (10+ Clients Proof)**
(回到**根目錄** `http_server/`)

```bash
./test_concurrent.sh
```

觀察伺服器終端機，你將會看到 11 個不同 PID 的子程序被建立並處理請求的 `[INFO]` 日誌。

**5. 封包擷取 (Packet Capture Proof)**
(開啟新終端機)

```bash
# 監聽 loopback 介面卡，-A 顯示 ASCII 內容
sudo tcpdump -i lo 'port 9999' -nn -A
```

執行 `./build/client /raminfo`，你將能在 `tcpdump` 中清楚看到 `GET /raminfo HTTP/1.1` 請求 和 `HTTP/1.1 200 OK` 回應。

## 6\. Auditing Discussion (健全性機制討論)

本專案實作了多種健全性機制。以下是三個「有/無」機制的行為差異範例：

**1. 範例：`SIGCHLD` (殭屍程序處理)**

  * **無健全性**：註解 `sigaction(SIGCHLD, ...)` 並重新編譯。執行 `test_concurrent.sh` 後，`ps aux | grep 'Z'` 會顯示大量 `<defunct>` 殭屍程序。
  * **有健全性**：`sigchld_handler` 會呼叫 `waitpid()` 自動回收子程序，`ps` 列表保持乾淨。

**2. 範例：`SIGPIPE` (客戶端崩潰處理)**

  * **無健全性**：註解 `signal(SIGPIPE, SIG_IGN)` 並重新編譯。當客戶端 在 `writen()` 完成前被 `kill -9`，對應的伺服器子程序 會因 `SIGPIPE` 訊號而**崩潰**。
  * **有健全性**：`writen()` 會安全地返回 `-1` (EPIPE)，子程序 會印出錯誤日誌 並乾淨地 `exit()`，不影響主伺服器。

**3. 範例：`SIGTSTP` (`Ctrl+Z` 處理)**

  * **無健全性**：註解 `signal(SIGTSTP, ...)` 並重新編譯。按下 `Ctrl+Z` 會「**暫停**」伺服器，導致 9999 埠持續被佔用，無法重啟。
  * **有健全性**：`termination_handler` 會捕捉 `SIGTSTP`，呼叫 `exit(0)` **乾淨地終止**伺服器，並釋放 9999 埠。
