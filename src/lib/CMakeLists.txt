# src/lib/CMakeLists.txt (修改後)

# 1. 建立 "http_lib" 函式庫
#    (注意：現在來源檔案有兩個)
add_library(http_lib SHARED
    logging.c
    robust_io.c
)

# 2. 告訴 CMake "公開" 標頭檔目錄 (保持不變)
target_include_directories(http_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 3. 將 Debug 旗標加入編譯 (保持不變)
target_compile_options(http_lib PRIVATE ${DEBUG_FLAG})