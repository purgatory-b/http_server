#!/bin/bash
# (要求 2) 並行連線測試腳本

# 檢查 client 執行檔是否存在於 build/ 目錄
CLIENT_EXE="./build/client"

if [ ! -f "$CLIENT_EXE" ]; then
    echo "Client executable not found at $CLIENT_EXE. Please run cmake and make first."
    exit 1
fi

CLIENT_COUNT=11
echo "Starting $CLIENT_COUNT concurrent clients..."

# 啟動 N 個客戶端到背景
for ((i=1; i<=CLIENT_COUNT; i++))
do
    echo "Starting client #$i"
    # 在背景執行，並將輸出導向 /dev/null 保持終端機乾淨
    $CLIENT_EXE > /dev/null 2>&1 &
done

echo "Waiting for all clients to finish..."
# 等待所有背景工作完成
wait

echo "$CLIENT_COUNT clients have completed."