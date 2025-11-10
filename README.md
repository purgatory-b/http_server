# 穩健的 C 語言 HTTP 伺服器 (Robust C HTTP Server)

本專案是一個使用 C 語言編寫的並行 (Concurrent) HTTP 1.1 伺服器，專注於實現 Linux 系統程式設計中的健全性 (Robustness) 機制。

伺服器採用 `fork()` (process-per-client) 的並行模型，確保每個客戶端連線都在一個獨立的行程中處理，實現了錯誤隔離。

專案的建構系統使用 `CMake`，並將所有共享功能（如日誌 和強健 I/O）封裝在一個動態共享函式庫 (`libhttp_lib.so`) 中。

## 核心特色 (Core Features)

  * **`fork()` 並行模型**：
    伺服器 在 `accept()` 之後會立即 `fork()` 一個子程序來處理連線，能同時服務 10+ 客戶端。

  * **HTTP 1.1 API 伺服器**：
    實作了一個可擴充的 API 伺服器，能解析 `GET` 請求 並依據 URI 進行路由。

  * **健全性機制 (Robustness)**：
    伺服器 實作了多種訊號處理機制 來處理異常狀況，包括：

    1.  **`sigaction(SIGCHLD, ...)`**：捕捉 `SIGCHLD` 訊號並呼叫 `waitpid()`，防止「殭屍程序」(Zombie Process) 產生。
    2.  **`signal(SIGPIPE, SIG_IGN)`**：忽略 `SIGPIPE` 訊號，防止子程序因客戶端突然斷線而崩潰。
    3.  **`if (errno == EINTR)`**：在 `accept()` 之後檢查「被中斷的系統呼叫」。
    4.  **`signal(SIGTSTP, ...)`**：捕捉 `Ctrl+Z` 訊號，呼叫 `exit()` 以乾淨地關閉伺服器並釋放埠號。

  * **動態共享函式庫 (`.so`)**：
    所有共享邏輯都被拆分並封裝在 `src/lib/` 目錄下（`logging.c` 和 `robust_io.c`），並被編譯為 `libhttp_lib.so`。

  * **兩階段日誌控制 (Two-Stage Logging)**：

    1.  **編譯時期**：`CMakeLists.txt` 會偵測 `CMAKE_BUILD_TYPE=Debug`，並定義 `DEBUG_BUILD` 宏。
    2.  **執行時期**：`logging.c` 會讀取 `MY_APP_DEBUG` 環境變數，來決定是否印出日誌。

## API 功能 (Available Endpoints)

伺服器 支援以下 `GET` 請求：

  * `GET /raminfo`：回傳伺服器記憶體資訊 (讀取 `/proc/meminfo`)。
  * `GET /sysinfo`：回傳 `uname -a` 的系統資訊。
  * `GET /loadavg`：回傳 `/proc/loadavg` 的系統負載。
  * `GET /time`：回傳 `Asia/Taipei` 的目前時間。
  * `GET /diskusage`：回傳 `df -h /` 的磁碟使用狀況。

## 專案結構 (Project Structure)

```
http_server/ (專案根目錄)
│
├── CMakeLists.txt          # 根 CMake 腳本
├── test_concurrent.sh      # 並行壓力測試腳本
├── README.md               # (本檔案)
├── .gitignore
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
    │
    └── lib/
        └── libhttp_lib.so      # (★ 最終的共享函式庫)
```

## 如何編譯 (CMake)

請勿在原始碼目錄中編譯。

1.  **建立 `build` 目錄**：

    ```bash
    mkdir build
    cd build
    ```

2.  **執行 CMake (產生建構檔)**：

      * **編譯 Debug 版本 (推薦，會印出日誌)**
        ```bash
        # 這會定義 -DDEBUG_BUILD
        cmake -DCMAKE_BUILD_TYPE=Debug ..
        ```
      * **編譯 Release 版本 (效能最高，不含日誌)**
        ```bash
        # 這「不會」定義 -DDEBUG_BUILD
        cmake -DCMAKE_BUILD_TYPE=Release ..
        ```

3.  **執行 Make (編譯程式)**：

    ```bash
    make
    ```

    編譯好的 `server` 和 `client` 執行檔會出現在 `build/` 目錄中。

## 如何執行與測試

**需要開啟多個終端機。**

### 1\. 啟動伺服器

(在 `build/` 目錄下)

```bash
# 啟動伺服器，並設定執行時期日誌等級為 1 (INFO)
MY_APP_DEBUG=1 ./server
```

伺服器將啟動並監聽 `http://127.0.0.1:9999`。

### 2\. 執行客戶端 (動態 URI)

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

### 3\. 使用 `curl` 測試

`curl` 是測試 HTTP 伺服器最快的方式。

```bash
# 測試系統負載
curl http://127.0.0.1:9999/loadavg

# 測試磁碟使用率
curl http://127.0.0.1:9999/diskusage
```

### 4\. 並行測試 (10+ Clients Proof)

(回到**根目錄** `http_server/`)

```bash
./test_concurrent.sh
```

觀察伺服器終端機，你將會看到 11 個不同 PID 的子程序被建立並處理請求的 `[INFO]` 日誌。

### 5\. 封包擷取 (Packet Capture Proof)

(開啟新終端機)

```bash
# 監聽 loopback 介面卡，-A 顯示 ASCII 內容
sudo tcpdump -i lo 'port 9999' -nn -A
```

執行 `./build/client /raminfo`，你將能在 `tcpdump` 中清楚看到 `GET /raminfo HTTP/1.1` 請求 和 `HTTP/1.1 200 OK` 回應。

```
```
