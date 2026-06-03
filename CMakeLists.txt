cmake_minimum_required(VERSION 4.0)
project(kwNet)


set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


find_package(OpenSSL REQUIRED)


set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(SOURCES
    ${SRC_DIR}/basics/Acceptor.cpp
    ${SRC_DIR}/basics/Channel.cpp
    ${SRC_DIR}/basics/Poller.cpp
    ${SRC_DIR}/Loggers/AsyncLogger.cpp
    ${SRC_DIR}/Loggers/FileUtil.cpp
    ${SRC_DIR}/Loggers/LogFile.cpp
    ${SRC_DIR}/Loggers/Logger.cpp
    ${SRC_DIR}/Loggers/LogStream.cpp
    ${SRC_DIR}/Memorys/Buffer.cpp
    ${SRC_DIR}/Sockets/InetAddr.cpp
    ${SRC_DIR}/Sockets/Socket.cpp
    ${SRC_DIR}/Sockets/SocketOps.cpp
    ${SRC_DIR}/Threads/CountDownLatch.cpp
    ${SRC_DIR}/Threads/CurrentThread.cpp
    ${SRC_DIR}/Threads/Thread.cpp
    ${SRC_DIR}/Threads/ThreadPool.cpp
    ${SRC_DIR}/Timers/Timer.cpp
    ${SRC_DIR}/Timers/TimerQueue.cpp
    ${SRC_DIR}/Timers/Timestamp.cpp
    ${SRC_DIR}/https/ssl_TcpConnection.cpp
    ${SRC_DIR}/https/ssl_handshake_handler.cpp
    ${SRC_DIR}/https/ssl_context.cpp
    ${SRC_DIR}/https/http_util.cpp
    ${SRC_DIR}/https/https_client.cpp
    ${SRC_DIR}/EventLoop.cpp
    ${SRC_DIR}/TcpClient.cpp
    ${SRC_DIR}/Connector.cpp
    ${SRC_DIR}/EventloopThread.cpp
    ${SRC_DIR}/EventloopThreadPool.cpp
    ${SRC_DIR}/TcpConnection.cpp
    ${SRC_DIR}/TcpServer.cpp
    ${SRC_DIR}/SignalHandler.cpp
)


add_library(network_static STATIC ${SOURCES})

add_library(network_dynamic SHARED ${SOURCES})


# target_include_directories(network_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(network_dynamic PUBLIC pthread rt OpenSSL::SSL OpenSSL::Crypto /home/kevin/dev/learning_network/third-party/llhttp/lib/libllhttp.so)
target_link_libraries(network_static PUBLIC pthread rt OpenSSL::SSL OpenSSL::Crypto)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(network_dynamic PUBLIC -Wall -Wextra -Wpedantic)
    target_compile_options(network_static PUBLIC -Wall -Wextra -Wpedantic -fPIC)
endif()


add_subdirectory(test)




