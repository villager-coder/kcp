CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)

project(kcp LANGUAGES C)

# 启用测试功能（配合 BUILD_TESTING 使用）
include(CTest)
# 使用标准的安装路径（如 lib, include, bin 等）
include(GNUInstallDirs)

# 设置策略 CMP0054，使得 if() 判断中的变量更精确，只对定义变量和字符串进行比较（防止不明确的行为）
cmake_policy(SET CMP0054 NEW)

# 条件编译: Windows 平台下共享库导出符号
# 如果是 构建共享库 且平台是 Windows，CMake 会生成一个 .def 文件列出需要导出的函数（因为 Windows 下共享库必须显式导出符号
if(BUILD_SHARED_LIBS AND WIN32)
    set(exports_def_file "${CMAKE_CURRENT_BINARY_DIR}/exports.def")
    set(exports_def_contents
"EXPORTS 
    ikcp_create
    ikcp_release
    ikcp_setoutput
    ikcp_recv
    ikcp_send
    ikcp_update
    ikcp_check
    ikcp_input
    ikcp_flush
    ikcp_peeksize
    ikcp_setmtu
    ikcp_wndsize
    ikcp_waitsnd
    ikcp_nodelay
    ikcp_log
    ikcp_allocator
    ikcp_getconv
")

    file(WRITE "${exports_def_file}" "${exports_def_contents}")
    # 添加源码 ikcp.c，同时加入 .def 文件
    add_library(kcp ikcp.c "${exports_def_file}")
else()
    # 否则，直接添加 ikcp.c 构建 kcp 库
    add_library(kcp ikcp.c)
endif()

# 安装头文件 ikcp.h 到系统的 include 目录（由 GNUInstallDirs 提供）
install(FILES ikcp.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# 安装 kcp 库
# 分别指定静态库（ARCHIVE）、动态库（LIBRARY）、可执行文件（RUNTIME）等的安装路径
# 设置导出文件 kcp-targets，供其他项目使用
install(TARGETS kcp
    EXPORT kcp-targets
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

# 使用 kcp:: 命名空间，导出一个 kcp-config.cmake，可用于其他项目通过 find_package(kcp) 方式查找
install(EXPORT kcp-targets
    FILE kcp-config.cmake
    NAMESPACE kcp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/kcp
)

# 如果启用了 BUILD_TESTING，则会构建测试程序
# 如果 MSVC 且版本大于 1900，则设置源码文件使用 UTF-8 编码
if(BUILD_TESTING)
    enable_language(CXX)

    add_executable(kcp_test test.cpp)
    if(MSVC AND NOT (MSVC_VERSION LESS 1900))
        target_compile_options(kcp_test PRIVATE /utf-8)
    endif()
endif()

# 配置: cmake -B build
# 编译: cmake --build build --config Debug --target ALL_BUILD -j 26
# 安装: cmake --install build --config Debug