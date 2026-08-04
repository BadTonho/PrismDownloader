cmake_minimum_required(VERSION 3.16)
project(PrismDownloader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Caminho opcional para uma instalação local do Qt. Pode ser sobrescrito via CMake.
set(PRISM_QT_ROOT "C:/Qt/6.7.2/msvc2019_64" CACHE PATH "Raiz da instalação do Qt usada pelo Prism Downloader")
if(PRISM_QT_ROOT AND EXISTS "${PRISM_QT_ROOT}")
    list(PREPEND CMAKE_PREFIX_PATH "${PRISM_QT_ROOT}")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Widgets Network REQUIRED)

add_executable(${PROJECT_NAME} WIN32
    app_icon.rc
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/MediaItem.h
    src/DownloadProfile.h
    src/GPUDetector.h
    src/GPUDetector.cpp
    src/DownloadManager.h
    src/DownloadManager.cpp
    src/ConversionManager.h
    src/ConversionManager.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Network)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(NOT PRISM_QT_ROOT OR NOT EXISTS "${PRISM_QT_ROOT}/bin/Qt6Core.dll")
        message(FATAL_ERROR "PRISM_QT_ROOT deve apontar para uma instalação Qt com os binários de runtime.")
    endif()
    # Copia automaticamente os novos ícones para junto do executável compilação após compilação
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app_icon.ico"
        "${CMAKE_CURRENT_SOURCE_DIR}/app_icon.png"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
    # Copia as DLLs do Qt para a pasta do executável para rodar sem falhas de dependência
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PRISM_QT_ROOT}/bin/Qt6Core.dll"
        "${PRISM_QT_ROOT}/bin/Qt6Gui.dll"
        "${PRISM_QT_ROOT}/bin/Qt6Widgets.dll"
        "${PRISM_QT_ROOT}/bin/Qt6Network.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
    # Copia a DLL de plataforma visual qwindows.dll para a subpasta platforms/
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PRISM_QT_ROOT}/plugins/platforms/qwindows.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms/"
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(PrismDownloaderTests
        tests/DownloadProfileTests.cpp
        src/DownloadProfile.h
        src/MediaItem.h
    )
    target_include_directories(PrismDownloaderTests PRIVATE src)
    target_compile_features(PrismDownloaderTests PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(PrismDownloaderTests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(PrismDownloaderTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME PrismDownloader.ProfileSelection COMMAND PrismDownloaderTests)

    add_executable(PrismFakeMediaTool tests/FakeMediaTool.cpp)
    target_link_libraries(PrismFakeMediaTool PRIVATE Qt6::Core)
    target_compile_features(PrismFakeMediaTool PRIVATE cxx_std_17)

    add_executable(PrismQueueManagerTests
        tests/QueueManagerTests.cpp
        src/DownloadManager.h
        src/DownloadManager.cpp
        src/ConversionManager.h
        src/ConversionManager.cpp
        src/DownloadProfile.h
        src/MediaItem.h
        src/GPUDetector.h
    )
    target_include_directories(PrismQueueManagerTests PRIVATE src)
    target_link_libraries(PrismQueueManagerTests PRIVATE Qt6::Core)
    target_compile_features(PrismQueueManagerTests PRIVATE cxx_std_17)
    if(MSVC)
        target_compile_options(PrismFakeMediaTool PRIVATE /W4 /permissive- /utf-8)
        target_compile_options(PrismQueueManagerTests PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(PrismFakeMediaTool PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(PrismQueueManagerTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME PrismDownloader.QueueManagers
             COMMAND PrismQueueManagerTests $<TARGET_FILE:PrismFakeMediaTool>)
endif()
