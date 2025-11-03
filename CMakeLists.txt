# http_server/CMakeLists.txt (新的)
cmake_minimum_required(VERSION 3.10)
project(robust_server C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED True)

# 設置建置類型 (Debug/Release)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type (Debug, Release, etc)" FORCE)
endif()

# 建立一個變數來儲存 Debug 旗標
set(DEBUG_FLAG "")
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # 這會定義 DEBUG_BUILD 宏
    set(DEBUG_FLAG "-DDEBUG_BUILD")
endif()

# 告訴 CMake 進入 'src/lib' 目錄去尋找函式庫
add_subdirectory(src/lib)

# --- 編譯 Server ---
# 指向 'src/server/server.c'
add_executable(server src/server/server.c)
# 連結函式庫 (我們在下一步會把函式庫目標改名為 'http_lib')
target_link_libraries(server PRIVATE http_lib)
target_compile_options(server PRIVATE ${DEBUG_FLAG})

# --- 編譯 Client ---
# 指向 'src/client/client.c'
add_executable(client src/client/client.c)
target_link_libraries(client PRIVATE http_lib)
target_compile_options(client PRIVATE ${DEBUG_FLAG})