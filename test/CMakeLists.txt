# 创建两个测试可执行文件
add_executable(EchoClient_unittest EchoClient_unittest.cpp)
add_executable(EchoServer_unittest EchoServer_unittest.cpp)
add_executable(AsyncLogger_test AsyncLogger_test.cpp)
add_executable(TimerQueue_test TimerQueue_test.cpp)

# 链接网络库（会传递 pthread 和 rt 依赖）
target_link_libraries(EchoClient_unittest PRIVATE network_lib)
target_link_libraries(EchoServer_unittest PRIVATE network_lib)
target_link_libraries(AsyncLogger_test PRIVATE network_lib)
target_link_libraries(TimerQueue_test PRIVATE network_lib)

# 添加头文件搜索路径（src 目录）
target_include_directories(EchoClient_unittest PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(EchoServer_unittest PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(AsyncLogger_test PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_include_directories(TimerQueue_test PRIVATE ${CMAKE_SOURCE_DIR}/src)

# 可选：为测试程序启用相同的编译警告
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(EchoClient_unittest PRIVATE -Wall -Wextra)
    target_compile_options(EchoServer_unittest PRIVATE -Wall -Wextra)
    target_compile_options(AsyncLogger_test PRIVATE -Wall -Wextra)
    target_compile_options(TimerQueue_test PRIVATE -Wall -Wextra)
endif()