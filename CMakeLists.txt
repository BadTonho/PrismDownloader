cmake_minimum_required(VERSION 3.16)
project(NeoVDownloader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Tenta localizar o Qt6, se não encontrar tenta o Qt5
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/MediaItem.h
    src/GPUDetector.h
    src/GPUDetector.cpp
    src/DownloadEngine.h
    src/DownloadEngine.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network)

if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CONSOLE)
endif()
