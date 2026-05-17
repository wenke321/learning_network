cmake_minimum_required(VERSION 4.0)
project(kwNet)


set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)


set(CMAKE_EXPORT_COMPILE_COMMANDS ON)


set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(SOURCES
    ${SRC_DIR}/Acceptor.cpp
    ${SRC_DIR}/AsyncLogger.cpp
    ${SRC_DIR}/Buffer.cpp
    ${SRC_DIR}/Channel.cpp
    ${SRC_DIR}/Connector.cpp
    ${SRC_DIR}/CountDownLatch.cpp
    ${SRC_DIR}/CurrentThread.cpp
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


add_library(network_static STATIC ${SOURCES}
    src/SignalHandler.cpp)

add_library(network_dynamic SHARED ${SOURCES}
    src/SignalHandler.cpp)


# target_include_directories(network_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(network_dynamic PUBLIC pthread rt)
target_link_libraries(network_static PUBLIC pthread rt)


if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(network_dynamic PRIVATE -Wall -Wextra -Wpedantic)
endif()


add_subdirectory(test)




