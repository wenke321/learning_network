cmake_minimum_required(VERSION 4.0)
project(kwNet)

# 使用 C++11 标准
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置输出目录：可执行文件放在 build/bin，库文件放在 build/lib
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 导出编译命令（用于 IDE 或 clangd）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 收集所有源文件（显式列出，避免 GLOB 的潜在问题）
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(SOURCES
    ${SRC_DIR}/Acceptor.cpp
    ${SRC_DIR}/AsyncLogger.cpp
    ${SRC_DIR}/Buffer.cpp
    ${SRC_DIR}/Channel.cpp
    ${SRC_DIR}/Connector.cpp
    ${SRC_DIR}/CountDownLatch.cpp
    ${SRC_DIR}/CurrentThread.cpp
    ${SRC_DIR}/Epoller.cpp
    ${SRC_DIR}/EventLoop.cpp
    ${SRC_DIR}/EventloopThread.cpp
    ${SRC_DIR}/EventloopThreadPool.cpp
    ${SRC_DIR}/FileUtil.cpp
    ${SRC_DIR}/InetAddr.cpp
    ${SRC_DIR}/LogFile.cpp
    ${SRC_DIR}/Logger.cpp
    ${SRC_DIR}/LogStream.cpp
    ${SRC_DIR}/Poller.cpp
    ${SRC_DIR}/Socket.cpp
    ${SRC_DIR}/SocketOps.cpp
    ${SRC_DIR}/TcpClient.cpp
    ${SRC_DIR}/TcpConnection.cpp
    ${SRC_DIR}/TcpServer.cpp
    ${SRC_DIR}/Thread.cpp
    ${SRC_DIR}/ThreadPool.cpp
    ${SRC_DIR}/Timer.cpp
    ${SRC_DIR}/TimerQueue.cpp
    ${SRC_DIR}/Timestamp.cpp
)

# 构建静态库
add_library(network_lib STATIC ${SOURCES})

# 公开头文件路径（src 目录），同时要求库的使用者链接 pthread 和 rt
target_include_directories(network_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(network_lib PUBLIC pthread rt)

# 可选编译选项（调试信息、警告）
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(network_lib PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 添加测试子目录
add_subdirectory(test)




