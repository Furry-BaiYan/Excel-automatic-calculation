cmake_minimum_required(VERSION 3.16)
project(ExcelProcessor VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 告诉 CMake Qt 装在哪里（改成你自己的路径）
set(CMAKE_PREFIX_PATH "D:/Qt/6.11.1/mingw_64")

# 找 Qt6 模块
find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Gui
    PrintSupport   # PDF 输出需要
)

# 引入 QXlsx（本地子目录方式，稳定不依赖网络）
add_subdirectory(QXlsx/QXlsx)

# 源文件列表
set(SOURCES
    src/main.cpp
)

# 生成可执行文件
add_executable(ExcelProcessor ${SOURCES})

# 链接库
target_link_libraries(ExcelProcessor PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::PrintSupport
    QXlsx::QXlsx
)

# Windows 下隐藏控制台黑窗口（正式版时开启）
# set_target_properties(ExcelProcessor PROPERTIES WIN32_EXECUTABLE TRUE)