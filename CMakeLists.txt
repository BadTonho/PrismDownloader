cmake_minimum_required(VERSION 3.16)
project(NeoVDownloader LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Aponta automaticamente para a instalação do Qt 6.7.2 no Windows
list(PREPEND CMAKE_PREFIX_PATH "C:/Qt/6.7.2/msvc2019_64")

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Widgets Network REQUIRED)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/MediaItem.h
    src/GPUDetector.h
    src/GPUDetector.cpp
    src/DownloadEngine.h
    src/DownloadEngine.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Network)

if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    # Copia as DLLs do Qt para a pasta do executável para rodar sem falhas de dependência
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Qt/6.7.2/msvc2019_64/bin/Qt6Core.dll"
        "C:/Qt/6.7.2/msvc2019_64/bin/Qt6Gui.dll"
        "C:/Qt/6.7.2/msvc2019_64/bin/Qt6Widgets.dll"
        "C:/Qt/6.7.2/msvc2019_64/bin/Qt6Network.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
    # Copia a DLL de plataforma visual qwindows.dll para a subpasta platforms/
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "C:/Qt/6.7.2/msvc2019_64/plugins/platforms/qwindows.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms/"
    )
endif()
