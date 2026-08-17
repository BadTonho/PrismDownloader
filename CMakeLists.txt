cmake_minimum_required(VERSION 3.16)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/version.iss" PRISM_VERSION_LINE
     REGEX "^#define PRISM_VERSION \"[0-9.]+\"$")
string(REGEX REPLACE "^#define PRISM_VERSION \"([0-9.]+)\"$" "\\1"
       PRISM_VERSION "${PRISM_VERSION_LINE}")
if(NOT PRISM_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "version.iss deve definir PRISM_VERSION no formato X.Y.Z")
endif()
project(PrismDownloader VERSION ${PRISM_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32 OR (UNIX AND NOT APPLE))
    option(PRISM_FETCH_BUNDLED_YTDLP "Baixar o yt-dlp Nightly fixado para o pacote" ON)
    set(PRISM_BUNDLED_YTDLP_PATH "" CACHE FILEPATH
        "Caminho alternativo para o yt-dlp Nightly validado")
    include(cmake/YtDlpBundle.cmake)
    prism_prepare_bundled_ytdlp(PRISM_BUNDLED_YTDLP_PATH)
endif()

# Caminho opcional para uma instalação local do Qt. No Linux, normalmente o Qt
# é descoberto pelas dependências instaladas pelo gerenciador de pacotes.
if(WIN32)
    set(PRISM_QT_ROOT "C:/Qt/6.7.2/msvc2019_64" CACHE PATH
        "Raiz da instalação do Qt usada pelo Prism Downloader")
else()
    set(PRISM_QT_ROOT "" CACHE PATH "Raiz opcional da instalação do Qt")
endif()
if(PRISM_QT_ROOT AND EXISTS "${PRISM_QT_ROOT}")
    list(PREPEND CMAKE_PREFIX_PATH "${PRISM_QT_ROOT}")
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 COMPONENTS Widgets Network REQUIRED)

configure_file(src/PrismVersion.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/PrismVersion.h" @ONLY)

set(PRISM_SOURCES
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/MediaItem.h
    src/DownloadProfile.h
    src/MediaToolResolver.h
    src/MediaToolResolver.cpp
    src/YtDlpUpdateService.h
    src/YtDlpUpdateService.cpp
    src/GPUDetector.h
    src/GPUDetector.cpp
    src/DownloadManager.h
    src/DownloadManager.cpp
    src/ConversionManager.h
    src/ConversionManager.cpp
    resources/app_icon.qrc
)

if(WIN32)
    add_executable(${PROJECT_NAME} WIN32 app_icon.rc ${PRISM_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${PRISM_SOURCES})
endif()

if(UNIX AND NOT APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME prism-downloader)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Widgets Qt6::Network)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
    if(NOT PRISM_QT_ROOT OR NOT EXISTS "${PRISM_QT_ROOT}/bin/Qt6Core.dll")
        message(FATAL_ERROR "PRISM_QT_ROOT deve apontar para uma instalação Qt com os binários de runtime.")
    endif()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app_icon.ico"
        "${CMAKE_CURRENT_SOURCE_DIR}/app_icon.png"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PRISM_QT_ROOT}/bin/Qt6Core.dll"
        "${PRISM_QT_ROOT}/bin/Qt6Gui.dll"
        "${PRISM_QT_ROOT}/bin/Qt6Widgets.dll"
        "${PRISM_QT_ROOT}/bin/Qt6Network.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PRISM_QT_ROOT}/plugins/platforms/qwindows.dll"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms/"
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(WIN32)
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PRISM_BUNDLED_YTDLP_PATH}"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/yt-dlp.exe"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/yt-dlp/NOTICE.md"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/YT-DLP-NOTICE.md"
    )
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

    add_executable(PrismMediaToolResolverTests
        tests/MediaToolResolverTests.cpp
        src/MediaToolResolver.h
        src/MediaToolResolver.cpp
    )
    target_include_directories(PrismMediaToolResolverTests PRIVATE src)
    target_link_libraries(PrismMediaToolResolverTests PRIVATE Qt6::Core)
    target_compile_features(PrismMediaToolResolverTests PRIVATE cxx_std_17)

    add_executable(PrismYtDlpUpdateServiceTests
        tests/YtDlpUpdateServiceTests.cpp
        src/YtDlpUpdateService.h
        src/YtDlpUpdateService.cpp
        src/MediaToolResolver.h
        src/MediaToolResolver.cpp
    )
    target_include_directories(PrismYtDlpUpdateServiceTests PRIVATE src)
    target_link_libraries(PrismYtDlpUpdateServiceTests PRIVATE Qt6::Core Qt6::Network)
    target_compile_features(PrismYtDlpUpdateServiceTests PRIVATE cxx_std_17)

    add_executable(PrismFakeMediaTool tests/FakeMediaTool.cpp)
    target_link_libraries(PrismFakeMediaTool PRIVATE Qt6::Core)
    target_compile_features(PrismFakeMediaTool PRIVATE cxx_std_17)

    add_executable(PrismQueueManagerTests
        tests/QueueManagerTests.cpp
        src/DownloadManager.h
        src/DownloadManager.cpp
        src/ConversionManager.h
        src/ConversionManager.cpp
        src/MediaToolResolver.h
        src/MediaToolResolver.cpp
        src/DownloadProfile.h
        src/MediaItem.h
        src/GPUDetector.h
    )
    target_include_directories(PrismQueueManagerTests PRIVATE src)
    target_link_libraries(PrismQueueManagerTests PRIVATE Qt6::Core)
    target_compile_features(PrismQueueManagerTests PRIVATE cxx_std_17)

    foreach(target PrismDownloaderTests PrismMediaToolResolverTests PrismYtDlpUpdateServiceTests PrismFakeMediaTool PrismQueueManagerTests)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()

    add_test(NAME PrismDownloader.ProfileSelection COMMAND PrismDownloaderTests)
    add_test(NAME PrismDownloader.MediaToolResolver COMMAND PrismMediaToolResolverTests)
    add_test(NAME PrismDownloader.YtDlpUpdateService COMMAND PrismYtDlpUpdateServiceTests)
    add_test(NAME PrismDownloader.QueueManagers
             COMMAND PrismQueueManagerTests $<TARGET_FILE:PrismFakeMediaTool>)
endif()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(PROGRAMS "${PRISM_BUNDLED_YTDLP_PATH}"
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/prism-downloader
            RENAME yt-dlp)
    install(FILES resources/prism-downloader.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES app_icon.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
            RENAME prism-downloader.png)
    install(FILES third_party/yt-dlp/NOTICE.md
            DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/prism-downloader
            RENAME YT-DLP-NOTICE.md)

    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_NAME "prism-downloader")
    set(CPACK_PACKAGE_VENDOR "Tonho Studios")
    set(CPACK_PACKAGE_CONTACT "Tonho Studios <contact@tonhostudios.com>")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Baixe e converta vídeos e áudios com yt-dlp e FFmpeg")
    set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Tonho Studios")
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "ffmpeg")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    include(CPack)
endif()
