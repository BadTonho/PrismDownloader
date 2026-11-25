cmake_minimum_required(VERSION 3.16)
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/version.iss" PRISM_VERSION_LINE
     REGEX "^#define PRISM_VERSION \"[0-9.]+\"$")
string(REGEX REPLACE "^#define PRISM_VERSION \"([0-9.]+)\"$" "\\1"
       PRISM_VERSION "${PRISM_VERSION_LINE}")
if(NOT PRISM_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "version.iss deve definir PRISM_VERSION no formato X.Y.Z")
endif()
project(PrismDownloader VERSION ${PRISM_VERSION} LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
option(PRISM_ENABLE_LTO "Ativar otimização de link-time no build de release" OFF)

if(UNIX AND NOT APPLE)
    # Formato de pacote deste build; o atualizador usa o valor para escolher o
    # asset correto da release (ver AppUpdateService::packageForLinuxId).
    set(PRISM_LINUX_PACKAGE "deb-ubuntu24.04" CACHE STRING
        "Formato Linux: deb-ubuntu24.04, deb-ubuntu22.04, deb-debian12, rpm ou bundle (AppImage/tar.gz)")
    set_property(CACHE PRISM_LINUX_PACKAGE PROPERTY STRINGS
        deb-ubuntu24.04 deb-ubuntu22.04 deb-debian12 rpm bundle)
    add_compile_definitions(PRISM_LINUX_PACKAGE="${PRISM_LINUX_PACKAGE}")
endif()

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
    set(PRISM_QT_ROOT "" CACHE PATH
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
include(cmake/FtxuiDependency.cmake)
configure_file(src/PrismVersion.h.in "${CMAKE_CURRENT_BINARY_DIR}/generated/PrismVersion.h" @ONLY)

set(PRISM_SOURCES
    src/main.cpp
    src/AppSettings.h
    src/AppSettings.cpp
    src/AppUpdateInstaller.h
    src/AppUpdateInstaller.cpp
    src/YtDlpStatus.h
    src/YtDlpStatus.cpp
    src/MainWindowUpdateCoordinator.h
    src/MainWindowUpdateCoordinator.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/MediaItem.h
    src/MediaMetadata.h
    src/MediaMetadata.cpp
    src/FormatSelectionDialog.h
    src/FormatSelectionDialog.cpp
    src/LibraryView.h
    src/LibraryView.cpp
    src/PlaylistItem.h
    src/PlaylistSelectionDialog.h
    src/PlaylistSelectionDialog.cpp
    src/PlaylistItemDetailsDialog.h
    src/PlaylistItemDetailsDialog.cpp
    src/PlaylistPreviewService.h
    src/PlaylistPreviewService.cpp
    src/PrismStyleSheet.h
    src/PrismStyleSheet.cpp
    src/LogHighlighter.h
    src/LogHighlighter.cpp
    src/MainWindowUiBuilder.h
    src/MainWindowUiBuilder.cpp
    src/DownloadQueueWorkflow.h
    src/DownloadQueueWorkflow.cpp
    src/YtDlpMetadataService.h
    src/YtDlpMetadataService.cpp
    src/DownloadProfile.h
    src/MediaToolResolver.h
    src/MediaToolResolver.cpp
    src/AppUpdateService.h
    src/AppUpdateService.cpp
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
    get_target_property(PRISM_QT_CORE_RUNTIME Qt6::Core IMPORTED_LOCATION_RELEASE)
    if(NOT PRISM_QT_CORE_RUNTIME)
        get_target_property(PRISM_QT_CORE_RUNTIME Qt6::Core IMPORTED_LOCATION)
    endif()
    if(PRISM_QT_CORE_RUNTIME)
        get_filename_component(PRISM_QT_BIN_DIR "${PRISM_QT_CORE_RUNTIME}" DIRECTORY)
    elseif(PRISM_QT_ROOT)
        set(PRISM_QT_BIN_DIR "${PRISM_QT_ROOT}/bin")
    endif()
    if(NOT PRISM_QT_BIN_DIR OR NOT EXISTS "${PRISM_QT_BIN_DIR}/Qt6Core.dll")
        message(FATAL_ERROR "Qt6Core.dll não foi localizado; defina PRISM_QT_ROOT ou CMAKE_PREFIX_PATH.")
    endif()
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/app_icon.ico"
        "${CMAKE_CURRENT_SOURCE_DIR}/app_icon.png"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PRISM_QT_BIN_DIR}/Qt6Core.dll"
        "${PRISM_QT_BIN_DIR}/Qt6Gui.dll"
        "${PRISM_QT_BIN_DIR}/Qt6Widgets.dll"
        "${PRISM_QT_BIN_DIR}/Qt6Network.dll"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>
    )
    add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PRISM_QT_BIN_DIR}/../plugins/platforms/qwindows.dll"
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
    target_compile_features(PrismDownloaderTests PRIVATE cxx_std_20)

    add_executable(PrismMediaMetadataTests
        tests/MediaMetadataTests.cpp
        src/MediaMetadata.h
        src/MediaMetadata.cpp
    )
    target_include_directories(PrismMediaMetadataTests PRIVATE src)
    target_link_libraries(PrismMediaMetadataTests PRIVATE Qt6::Core)
    target_compile_features(PrismMediaMetadataTests PRIVATE cxx_std_20)

    add_executable(PrismFormatSelectionDialogTests
        tests/FormatSelectionDialogTests.cpp
        src/FormatSelectionDialog.h
        src/FormatSelectionDialog.cpp
        src/MediaMetadata.h
        src/MediaMetadata.cpp
    )
    target_include_directories(PrismFormatSelectionDialogTests PRIVATE src)
    target_link_libraries(PrismFormatSelectionDialogTests PRIVATE Qt6::Widgets Qt6::Network)
    target_compile_features(PrismFormatSelectionDialogTests PRIVATE cxx_std_20)

    add_executable(PrismMediaToolResolverTests
        tests/MediaToolResolverTests.cpp
        src/MediaToolResolver.h
        src/MediaToolResolver.cpp
    )
    target_include_directories(PrismMediaToolResolverTests PRIVATE src)
    target_link_libraries(PrismMediaToolResolverTests PRIVATE Qt6::Core)
    target_compile_features(PrismMediaToolResolverTests PRIVATE cxx_std_20)

    add_executable(PrismYtDlpUpdateServiceTests
        tests/YtDlpUpdateServiceTests.cpp
        src/YtDlpUpdateService.h
        src/YtDlpUpdateService.cpp
        src/MediaToolResolver.h
        src/MediaToolResolver.cpp
    )
    target_include_directories(PrismYtDlpUpdateServiceTests PRIVATE src)
    target_link_libraries(PrismYtDlpUpdateServiceTests PRIVATE Qt6::Core Qt6::Network)
    target_compile_features(PrismYtDlpUpdateServiceTests PRIVATE cxx_std_20)

    add_executable(PrismAppUpdateServiceTests
        tests/AppUpdateServiceTests.cpp
        src/AppUpdateService.h
        src/AppUpdateService.cpp
    )
    target_include_directories(PrismAppUpdateServiceTests PRIVATE src)
    target_link_libraries(PrismAppUpdateServiceTests PRIVATE Qt6::Core Qt6::Network)
    target_compile_features(PrismAppUpdateServiceTests PRIVATE cxx_std_20)

    add_executable(PrismPortableUpdateCommonTests
        tests/PortableUpdateCommonTests.cpp
        src/PortableUpdateCommon.h
        src/PortableUpdateCommon.cpp
    )
    target_include_directories(PrismPortableUpdateCommonTests PRIVATE src)
    target_link_libraries(PrismPortableUpdateCommonTests PRIVATE Qt6::Core)
    target_compile_features(PrismPortableUpdateCommonTests PRIVATE cxx_std_20)

    add_executable(PrismFakeMediaTool tests/FakeMediaTool.cpp)
    target_link_libraries(PrismFakeMediaTool PRIVATE Qt6::Core)
    target_compile_features(PrismFakeMediaTool PRIVATE cxx_std_20)

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
    target_compile_features(PrismQueueManagerTests PRIVATE cxx_std_20)

    add_executable(PrismTuiStateTests
        tests/TuiStateTests.cpp
        src/tui/TuiState.h
        src/tui/TuiState.cpp
        src/MediaItem.h
    )
    target_include_directories(PrismTuiStateTests PRIVATE src)
    target_link_libraries(PrismTuiStateTests PRIVATE Qt6::Core)
    target_compile_features(PrismTuiStateTests PRIVATE cxx_std_20)

    foreach(target PrismDownloaderTests PrismMediaMetadataTests PrismFormatSelectionDialogTests PrismMediaToolResolverTests PrismYtDlpUpdateServiceTests PrismAppUpdateServiceTests PrismPortableUpdateCommonTests PrismFakeMediaTool PrismQueueManagerTests PrismTuiStateTests)
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PRISM_QT_BIN_DIR}/Qt6Core.dll"
                "${PRISM_QT_BIN_DIR}/Qt6Gui.dll"
                "${PRISM_QT_BIN_DIR}/Qt6Widgets.dll"
                "${PRISM_QT_BIN_DIR}/Qt6Network.dll"
                $<TARGET_FILE_DIR:${target}>
            )
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        endif()
    endforeach()

    add_test(NAME PrismDownloader.ProfileSelection COMMAND PrismDownloaderTests)
    add_test(NAME PrismDownloader.MediaMetadata COMMAND PrismMediaMetadataTests)
    add_test(NAME PrismDownloader.FormatSelectionDialog COMMAND PrismFormatSelectionDialogTests)
    set_tests_properties(PrismDownloader.FormatSelectionDialog PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    add_test(NAME PrismDownloader.MediaToolResolver COMMAND PrismMediaToolResolverTests)
    add_test(NAME PrismDownloader.YtDlpUpdateService COMMAND PrismYtDlpUpdateServiceTests)
    add_test(NAME PrismDownloader.AppUpdateService COMMAND PrismAppUpdateServiceTests)
    add_test(NAME PrismDownloader.PortableUpdateCommon COMMAND PrismPortableUpdateCommonTests)
    add_test(NAME PrismDownloader.QueueManagers
             COMMAND PrismQueueManagerTests $<TARGET_FILE:PrismFakeMediaTool>)
    add_test(NAME PrismDownloader.TuiState COMMAND PrismTuiStateTests)
endif()

add_executable(prism-cli
    src/tui/TuiMain.cpp
    src/tui/TuiApp.h
    src/tui/TuiApp.cpp
    src/tui/TuiBackend.h
    src/tui/TuiBackend.cpp
    src/tui/TuiState.h
    src/tui/TuiState.cpp
    src/AppSettings.h
    src/AppSettings.cpp
    src/DownloadManager.h
    src/DownloadManager.cpp
    src/ConversionManager.h
    src/ConversionManager.cpp
    src/MediaToolResolver.h
    src/MediaToolResolver.cpp
    src/GPUDetector.h
    src/GPUDetector.cpp
    src/DownloadProfile.h
    src/MediaItem.h
)
target_include_directories(prism-cli PRIVATE
    src
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
)
target_link_libraries(prism-cli PRIVATE
    Qt6::Core
    ftxui::screen
    ftxui::dom
    ftxui::component
)
target_compile_features(prism-cli PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(prism-cli PRIVATE /W4 /permissive- /utf-8)
    target_compile_definitions(prism-cli PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    add_custom_command(TARGET prism-cli POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PRISM_QT_BIN_DIR}/Qt6Core.dll"
        $<TARGET_FILE_DIR:prism-cli>
    )
else()
    target_compile_options(prism-cli PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(PRISM_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT PRISM_IPO_SUPPORTED OUTPUT PRISM_IPO_ERROR)
    if(PRISM_IPO_SUPPORTED)
        set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        set_property(TARGET ${PROJECT_NAME} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    else()
        message(WARNING "LTO solicitado, mas não suportado: ${PRISM_IPO_ERROR}")
    endif()
endif()

if(WIN32)
    add_executable(PrismPortableUpdateHelper WIN32
        src/PortableUpdateHelper.cpp
        src/PortableUpdateCommon.h
        src/PortableUpdateCommon.cpp
    )
    target_link_libraries(PrismPortableUpdateHelper PRIVATE Qt6::Core)
    target_compile_features(PrismPortableUpdateHelper PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(PrismPortableUpdateHelper PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(PrismPortableUpdateHelper PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(BUILD_TESTING AND MSVC)
        add_custom_command(TARGET PrismFormatSelectionDialogTests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:PrismFormatSelectionDialogTests>/platforms"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PRISM_QT_BIN_DIR}/../plugins/platforms/qoffscreen.dll"
            "$<TARGET_FILE_DIR:PrismFormatSelectionDialogTests>/platforms/qoffscreen.dll"
        )
    endif()
endif()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS ${PROJECT_NAME} prism-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    # Caminho fixo: o app procura ../lib/prism-downloader/yt-dlp (em Fedora o
    # CMAKE_INSTALL_LIBDIR seria lib64 e o binário não seria encontrado).
    install(PROGRAMS "${PRISM_BUNDLED_YTDLP_PATH}"
            DESTINATION lib/prism-downloader
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
    # AMD/Intel VAAPI uses the Mesa driver on the supported Ubuntu/Mint targets.
    # Keep this optional so NVIDIA-only systems are not made to depend on it.
    set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "mesa-va-drivers")

    # RPM (Fedora/openSUSE). O ffmpeg vem de repositórios diferentes (ffmpeg-free,
    # RPM Fusion, Packman), então a dependência é pelo arquivo.
    set(CPACK_RPM_PACKAGE_LICENSE "GPLv3")
    set(CPACK_RPM_PACKAGE_ARCHITECTURE "x86_64")
    set(CPACK_RPM_PACKAGE_REQUIRES "/usr/bin/ffmpeg")
    set(CPACK_RPM_PACKAGE_RELEASE 1)
    set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
    # O yt-dlp embutido é um executável PyInstaller; o strip/brp do rpmbuild
    # corromperia o payload.
    set(CPACK_RPM_SPEC_MORE_DEFINE "%define __os_install_post %{nil}\n%define _build_id_links none")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
    include(CPack)
endif()
