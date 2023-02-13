# 指定CMAKE的最低版本
CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

# 【需编译的源代码文件】
#SET(target_file ${CMAKE_SOURCE_DIR}/test/test_thread)
#SET(target_file ./test/test_thread)
SET(target_file test_socket)

# 指定生成文件的名称
PROJECT(${target_file})

# 【需引用的库名】
SET(link_file yaml-cpp pthread dl)

# 包含头文件搜索目录
SET(inc_dir_1 ${CMAKE_SOURCE_DIR}/inc)
INCLUDE_DIRECTORIES(${inc_dir_1})


# AUX_SOURCE_DIRECTORY(. test_src)
SET(test_src ${target_file}.cpp log.cpp config.cpp thread.cpp mutex.cpp util.cpp fiber.cpp scheduler.cpp iomanager.cpp timer.cpp hook.cpp fd_manager.cpp endian.cpp address.cpp socket.cpp)
# SET(test_src ${target_file}.cpp log.cpp config.cpp thread.cpp mutex.cpp util.cpp fiber.cpp scheduler.cpp iomanager.cpp timer.cpp)


# 【生成的可执行文件放置目录】
SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build/bin)

# 生成可执行文件
ADD_EXECUTABLE(${target_file} ${test_src})

# 链接动态库
TARGET_LINK_LIBRARIES(${target_file} ${link_file})
