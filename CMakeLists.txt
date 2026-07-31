cmake_minimum_required(VERSION 3.16)
project(NeoVDownloaderCore LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/MediaItem.h
    src/GPUDetector.h
    src/GPUDetector.cpp
    src/DownloadEngine.h
    src/DownloadEngine.cpp
)

if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CONSOLE _CRT_SECURE_NO_WARNINGS)
endif()
