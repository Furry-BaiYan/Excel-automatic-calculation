cmake_minimum_required(VERSION 3.16)
project(ExcelProcessor VERSION 1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 加这三行
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_PREFIX_PATH "D:/Qt/6.11.1/mingw_64")

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Widgets
    Gui
    PrintSupport
)

add_subdirectory(QXlsx/QXlsx)

set(SOURCES
    src/main.cpp
    src/io/XlsxReader.cpp
    src/io/XlsxWriter.cpp
    src/io/PdfExporter.cpp
    src/core/FormulaEngine.cpp
    src/core/StatEngine.cpp
    src/config/ProcessRule.cpp
    src/config/RuleEditor.cpp
)

add_executable(ExcelProcessor ${SOURCES})

target_link_libraries(ExcelProcessor PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Gui
    Qt6::PrintSupport
    QXlsx::QXlsx
)