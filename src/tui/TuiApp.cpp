#include "TuiApp.h"

#include "TuiBackend.h"
#include "TuiState.h"

#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using namespace ftxui;

const Color kEmerald = Color::RGB(16, 185, 129);
const Color kEmeraldBright = Color::RGB(52, 211, 153);
const Color kCyan = Color::RGB(56, 189, 248);
const Color kAmber = Color::RGB(245, 158, 11);
const Color kRed = Color::RGB(248, 113, 113);
const Color kSlateText = Color::RGB(226, 232, 240);
const Color kSlateMuted = Color::RGB(148, 163, 184);
const Color kBorderColor = Color::RGB(42, 58, 76);

Color statusColor(DownloadStatus status)
{
    switch (status) {
    case DownloadStatus::Completed:
        return kEmeraldBright;
    case DownloadStatus::Downloading:
    case DownloadStatus::Muxing:
    case DownloadStatus::ConvertingGPU:
        return kCyan;
    case DownloadStatus::Queued:
    case DownloadStatus::Cancelling:
        return kAmber;
    case DownloadStatus::Error:
    case DownloadStatus::Cancelled:
        return kRed;
    }
    return kSlateMuted;
}

std::string formatPercent(double value)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%5.1f%%", std::clamp(value, 0.0, 100.0));
    return std::string(buf);
}

std::string truncateText(const std::string &text, std::size_t maxLen)
{
    if (text.size() <= maxLen || maxLen < 4) {
        return text;
    }
    return text.substr(0, maxLen - 3) + "...";
}

ButtonOption primaryButtonStyle()
{
    ButtonOption opt = ButtonOption::Animated(Color::RGB(16, 185, 129),
                                              Color::RGB(4, 24, 16),
                                              Color::RGB(52, 211, 153),
                                              Color::RGB(2, 20, 14));
    return opt;
}

ButtonOption secondaryButtonStyle()
{
    ButtonOption opt = ButtonOption::Animated(Color::RGB(22, 31, 43),
                                              Color::RGB(226, 232, 240),
                                              Color::RGB(30, 44, 62),
                                              Color::RGB(52, 211, 153));
    return opt;
}

ButtonOption dangerButtonStyle()
{
    ButtonOption opt = ButtonOption::Animated(Color::RGB(42, 22, 27),
                                              Color::RGB(248, 113, 113),
                                              Color::RGB(220, 38, 38),
                                              Color::RGB(255, 255, 255));
    return opt;
}

} // namespace

TuiApp::TuiApp(TuiState &state, TuiBackend &backend, std::string versionTag)
    : m_state(state),
      m_backend(backend),
      m_versionTag(std::move(versionTag))
{
}

int TuiApp::run()
{
    auto screen = ScreenInteractive::Fullscreen();
    m_backend.setStateChangedCallback([&screen]() {
        screen.PostEvent(Event::Custom);
    });

    const TuiSnapshot initial = m_state.snapshot();

    // Navigation state
    int activeTab = 0;
    const std::vector<std::string> navEntries = {
        "1. Downloads",
        "2. Biblioteca",
        "3. Conversor",
        "4. Logs",
        "5. Sistema"
    };

    // Downloads state
    std::string urlText;
    std::string outputDirText = initial.outputFolder;
    const std::vector<std::string> qualityEntries = TuiState::qualityPresets();
    int selectedQuality = 0;
    const std::vector<std::string> concurrencyEntries = {"1", "2", "3", "4", "5"};
    int selectedConcurrency = std::clamp(initial.concurrencyLimit - 1, 0, 4);
    int selectedQueueIndex = 0;

    // Library state
    int selectedLibraryIndex = 0;

    // Converter state
    std::string convertInputText;
    const std::vector<std::string> convertFormats = TuiState::converterFormats();
    int selectedConvertFormat = 0;

    // Logs state
    const std::vector<std::string> logFilterEntries = {"Todos", "Processos", "Erros"};
    int selectedLogFilter = 0;

    // --- Sidebar menu ---
    MenuOption navOption = MenuOption::Vertical();
    navOption.entries_option.transform = [](const EntryState &entry) {
        Element elem = text((entry.active ? " > " : "   ") + entry.label + " ");
        if (entry.active) {
            elem = elem | bold | color(kEmeraldBright) | bgcolor(Color::RGB(19, 40, 34));
        } else if (entry.focused) {
            elem = elem | color(kSlateText) | bgcolor(Color::RGB(23, 32, 44));
        } else {
            elem = elem | color(kSlateMuted);
        }
        return elem;
    };
    Component navMenu = Menu(&navEntries, &activeTab, navOption);

    Component quitBtn = Button(" Sair (Ctrl+Q) ", screen.ExitLoopClosure(), dangerButtonStyle());

    // --- Tab 0: Downloads ---
    auto triggerEnqueue = [&]() {
        if (urlText.empty()) {
            m_state.setBanner("Cole uma URL valida antes de adicionar a fila.", true);
            screen.PostEvent(Event::Custom);
            return;
        }
        const std::string quality = qualityEntries.at(static_cast<std::size_t>(selectedQuality));
        m_backend.enqueueDownload(urlText, quality, outputDirText);
        urlText.clear();
    };

    InputOption urlInputOpt;
    urlInputOpt.placeholder = "https://youtube.com/watch?v=... (Enter para enfileirar)";
    urlInputOpt.multiline = false;
    urlInputOpt.on_enter = triggerEnqueue;
    Component urlInput = Input(&urlText, urlInputOpt);

    InputOption dirInputOpt;
    dirInputOpt.placeholder = "Pasta de destino dos downloads";
    dirInputOpt.multiline = false;
    dirInputOpt.on_enter = [&]() {
        m_backend.setOutputFolder(outputDirText);
    };
    Component outputDirInput = Input(&outputDirText, dirInputOpt);

    Component addBtn = Button(" Adicionar a Fila ", triggerEnqueue, primaryButtonStyle());
    Component applyDirBtn = Button(" Aplicar Pasta ", [&]() {
        m_backend.setOutputFolder(outputDirText);
    }, secondaryButtonStyle());

    RadioboxOption qualityOpt;
    Component qualityPicker = Radiobox(&qualityEntries, &selectedQuality, qualityOpt);

    MenuOption concOpt = MenuOption::Horizontal();
    concOpt.on_change = [&]() {
        m_backend.setConcurrency(selectedConcurrency + 1);
    };
    concOpt.entries_option.transform = [](const EntryState &entry) {
        Element elem = text(" " + entry.label + " ");
        if (entry.active) {
            elem = elem | bold | color(Color::RGB(4, 24, 16)) | bgcolor(kEmerald);
        } else {
            elem = elem | color(kSlateMuted);
        }
        return elem;
    };
    Component concurrencyPicker = Menu(&concurrencyEntries, &selectedConcurrency, concOpt);

    Component queueUpBtn = Button(" Item Anterior ", [&]() {
        if (selectedQueueIndex > 0) {
            --selectedQueueIndex;
        }
    }, secondaryButtonStyle());

    Component queueDownBtn = Button(" Proximo Item ", [&]() {
        const auto snap = m_state.snapshot();
        if (!snap.queue.empty() && selectedQueueIndex + 1 < static_cast<int>(snap.queue.size())) {
            ++selectedQueueIndex;
        }
    }, secondaryButtonStyle());

    Component cancelSelectedBtn = Button(" Cancelar Selecionado ", [&]() {
        const auto snap = m_state.snapshot();
        if (selectedQueueIndex >= 0 && selectedQueueIndex < static_cast<int>(snap.queue.size())) {
            m_backend.cancelDownload(snap.queue.at(static_cast<std::size_t>(selectedQueueIndex)).id);
        }
    }, dangerButtonStyle());

    Component cancelAllBtn = Button(" Cancelar Todos ", [&]() {
        m_backend.cancelAllDownloads();
    }, dangerButtonStyle());

    Component downloadsContainer = Container::Vertical({
        Container::Horizontal({urlInput, addBtn}),
        Container::Horizontal({outputDirInput, applyDirBtn}),
        qualityPicker,
        concurrencyPicker,
        Container::Horizontal({queueUpBtn, queueDownBtn, cancelSelectedBtn, cancelAllBtn}),
    });

    Component downloadsView = Renderer(downloadsContainer, [&]() {
        const TuiSnapshot snap = m_state.snapshot();
        if (selectedQueueIndex >= static_cast<int>(snap.queue.size())) {
            selectedQueueIndex = std::max(0, static_cast<int>(snap.queue.size()) - 1);
        }

        Elements queueRows;
        if (snap.queue.empty()) {
            queueRows.push_back(
                text("  Nenhum item na fila. Cole uma URL acima e pressione Enter.") | color(kSlateMuted));
        } else {
            for (int i = 0; i < static_cast<int>(snap.queue.size()); ++i) {
                const TuiQueueItem &item = snap.queue.at(static_cast<std::size_t>(i));
                const bool isSelected = (i == selectedQueueIndex);
                const std::string marker = isSelected ? " > " : "   ";
                const std::string title = truncateText(item.title, 42);
                const float ratio = static_cast<float>(std::clamp(item.progress, 0.0, 100.0) / 100.0);

                Element row = hbox({
                    text(marker + "#" + std::to_string(item.id) + " ") | bold | color(isSelected ? kEmeraldBright : kSlateMuted),
                    text(title) | color(kSlateText) | size(WIDTH, EQUAL, 42),
                    text("  "),
                    text(truncateText(item.quality, 14)) | color(kSlateMuted) | size(WIDTH, EQUAL, 14),
                    text("  "),
                    gauge(ratio) | color(statusColor(item.status)) | size(WIDTH, EQUAL, 16),
                    text(" " + formatPercent(item.progress) + " ") | bold | color(kSlateText),
                    text(item.speed.empty() ? "--" : item.speed) | color(kCyan) | size(WIDTH, EQUAL, 11),
                    text(" "),
                    text(item.statusText.empty() ? TuiState::statusLabel(item.status) : truncateText(item.statusText, 16))
                        | bold | color(statusColor(item.status)),
                });

                if (isSelected) {
                    row = row | bgcolor(Color::RGB(20, 40, 34));
                }
                queueRows.push_back(row);
            }
        }

        return vbox({
            vbox({
                text(" ADICIONAR DOWNLOAD ") | bold | color(kEmeraldBright),
                hbox({
                    text(" URL:   ") | bold | color(kSlateMuted) | center,
                    urlInput->Render() | borderStyled(LIGHT, kBorderColor) | flex,
                    text(" "),
                    addBtn->Render() | center,
                }),
                hbox({
                    text(" Pasta: ") | bold | color(kSlateMuted) | center,
                    outputDirInput->Render() | borderStyled(LIGHT, kBorderColor) | flex,
                    text(" "),
                    applyDirBtn->Render() | center,
                }),
            }) | borderStyled(ROUNDED, kBorderColor),

            hbox({
                vbox({
                    text(" QUALIDADE / FORMATO ") | bold | color(kEmeraldBright),
                    qualityPicker->Render(),
                }) | borderStyled(ROUNDED, kBorderColor) | flex,
                text(" "),
                vbox({
                    text(" CONTROLES DA FILA ") | bold | color(kEmeraldBright),
                    hbox({
                        text(" Simultaneos: ") | color(kSlateMuted) | center,
                        concurrencyPicker->Render(),
                    }),
                    separatorEmpty(),
                    hbox({
                        text(" Ativos: ") | color(kSlateMuted),
                        text(std::to_string(snap.activeDownloads)) | bold | color(kEmeraldBright),
                        text("   Aguardando: ") | color(kSlateMuted),
                        text(std::to_string(snap.pendingDownloads)) | bold | color(kAmber),
                    }),
                    separatorEmpty(),
                    hbox({
                        queueUpBtn->Render(),
                        text(" "),
                        queueDownBtn->Render(),
                    }),
                    separatorEmpty(),
                    hbox({
                        cancelSelectedBtn->Render(),
                        text(" "),
                        cancelAllBtn->Render(),
                    }),
                }) | borderStyled(ROUNDED, kBorderColor),
            }),

            vbox({
                text(" FILA DE DOWNLOADS ") | bold | color(kEmeraldBright),
                separator() | color(kBorderColor),
                vbox(std::move(queueRows)) | yframe | flex,
            }) | borderStyled(ROUNDED, kBorderColor) | flex,
        });
    });

    // --- Tab 1: Biblioteca ---
    Component libRefreshBtn = Button(" Atualizar Lista ", [&]() {
        m_backend.refreshLibrary(outputDirText);
    }, secondaryButtonStyle());

    Component libPrevBtn = Button(" Anterior ", [&]() {
        if (selectedLibraryIndex > 0) {
            --selectedLibraryIndex;
        }
    }, secondaryButtonStyle());

    Component libNextBtn = Button(" Proximo ", [&]() {
        const auto snap = m_state.snapshot();
        if (!snap.library.empty() && selectedLibraryIndex + 1 < static_cast<int>(snap.library.size())) {
            ++selectedLibraryIndex;
        }
    }, secondaryButtonStyle());

    Component libSendToConverterBtn = Button(" Enviar para Conversor ", [&]() {
        const auto snap = m_state.snapshot();
        if (selectedLibraryIndex >= 0 && selectedLibraryIndex < static_cast<int>(snap.library.size())) {
            convertInputText = snap.library.at(static_cast<std::size_t>(selectedLibraryIndex)).fullPath;
            activeTab = 2;
            m_state.setBanner("Arquivo carregado no Conversor.", false);
            screen.PostEvent(Event::Custom);
        }
    }, primaryButtonStyle());

    Component libraryContainer = Container::Horizontal({
        libRefreshBtn, libPrevBtn, libNextBtn, libSendToConverterBtn
    });

    Component libraryView = Renderer(libraryContainer, [&]() {
        const TuiSnapshot snap = m_state.snapshot();
        if (selectedLibraryIndex >= static_cast<int>(snap.library.size())) {
            selectedLibraryIndex = std::max(0, static_cast<int>(snap.library.size()) - 1);
        }

        Elements rows;
        if (snap.library.empty()) {
            rows.push_back(text("  Nenhum arquivo de midia encontrado em: " + snap.outputFolder) | color(kSlateMuted));
        } else {
            for (int i = 0; i < static_cast<int>(snap.library.size()); ++i) {
                const TuiLibraryItem &item = snap.library.at(static_cast<std::size_t>(i));
                const bool isSelected = (i == selectedLibraryIndex);
                Element row = hbox({
                    text(isSelected ? " > " : "   ") | bold | color(kEmeraldBright),
                    text(truncateText(item.name, 56)) | color(kSlateText) | flex,
                    text("  "),
                    text(item.extension) | bold | color(kCyan) | size(WIDTH, EQUAL, 6),
                    text("  "),
                    text(TuiState::formatFileSize(item.sizeBytes)) | color(kSlateMuted) | size(WIDTH, EQUAL, 12),
                });
                if (isSelected) {
                    row = row | bgcolor(Color::RGB(20, 40, 34));
                }
                rows.push_back(row);
            }
        }

        return vbox({
            hbox({
                libRefreshBtn->Render(),
                text(" "),
                libPrevBtn->Render(),
                text(" "),
                libNextBtn->Render(),
                text(" "),
                libSendToConverterBtn->Render(),
                filler(),
                text("Pasta: " + truncateText(snap.outputFolder, 40)) | color(kSlateMuted) | center,
            }) | borderStyled(ROUNDED, kBorderColor),
            vbox({
                text(" BIBLIOTECA DE MIDIAS (" + std::to_string(snap.library.size()) + " arquivo(s)) ")
                    | bold | color(kEmeraldBright),
                separator() | color(kBorderColor),
                vbox(std::move(rows)) | yframe | flex,
            }) | borderStyled(ROUNDED, kBorderColor) | flex,
        });
    });

    // --- Tab 2: Conversor ---
    InputOption convInputOpt;
    convInputOpt.placeholder = "Caminho completo do arquivo de video ou audio...";
    convInputOpt.multiline = false;
    Component convertInput = Input(&convertInputText, convInputOpt);

    Component convertFormatPicker = Radiobox(&convertFormats, &selectedConvertFormat);

    Component startConvertBtn = Button(" Iniciar Conversao ", [&]() {
        const std::string format = convertFormats.at(static_cast<std::size_t>(selectedConvertFormat));
        m_backend.startConversion(convertInputText, format, outputDirText);
    }, primaryButtonStyle());

    Component cancelConvertBtn = Button(" Cancelar Conversao ", [&]() {
        m_backend.cancelConversion();
    }, dangerButtonStyle());

    Component converterContainer = Container::Vertical({
        convertInput,
        convertFormatPicker,
        Container::Horizontal({startConvertBtn, cancelConvertBtn}),
    });

    Component converterView = Renderer(converterContainer, [&]() {
        const TuiSnapshot snap = m_state.snapshot();
        const float ratio = static_cast<float>(std::clamp(snap.converter.progress, 0.0, 100.0) / 100.0);

        return vbox({
            vbox({
                text(" CONVERSOR NATIVO DE MIDIA (FFMPEG + GPU) ") | bold | color(kEmeraldBright),
                hbox({
                    text(" Origem: ") | bold | color(kSlateMuted) | center,
                    convertInput->Render() | borderStyled(LIGHT, kBorderColor) | flex,
                }),
                separatorEmpty(),
                text(" Formato de Saida:") | bold | color(kSlateMuted),
                convertFormatPicker->Render(),
                separatorEmpty(),
                hbox({
                    text(" Motor: ") | bold | color(kSlateMuted),
                    text(snap.system.gpuName + " (" + snap.system.gpuCodec + ")")
                        | bold | color(snap.system.hardwareAcceleration ? kEmeraldBright : kAmber),
                }),
                separatorEmpty(),
                hbox({
                    startConvertBtn->Render(),
                    text(" "),
                    cancelConvertBtn->Render(),
                }),
            }) | borderStyled(ROUNDED, kBorderColor),

            vbox({
                text(" PROGRESSO DA CONVERSAO ") | bold | color(kEmeraldBright),
                text(" " + snap.converter.statusText) | color(kCyan),
                separatorEmpty(),
                hbox({
                    gauge(ratio) | color(kEmeraldBright) | flex,
                    text(" " + formatPercent(snap.converter.progress)) | bold | color(kSlateText),
                }),
            }) | borderStyled(ROUNDED, kBorderColor),
            filler(),
        });
    });

    // --- Tab 3: Logs ---
    MenuOption logFilterOpt = MenuOption::Horizontal();
    logFilterOpt.entries_option.transform = [](const EntryState &entry) {
        Element elem = text(" " + entry.label + " ");
        if (entry.active) {
            elem = elem | bold | color(Color::RGB(4, 24, 16)) | bgcolor(kEmerald);
        } else {
            elem = elem | color(kSlateMuted);
        }
        return elem;
    };
    Component logFilterMenu = Menu(&logFilterEntries, &selectedLogFilter, logFilterOpt);

    Component clearLogsBtn = Button(" Limpar Logs ", [&]() {
        m_state.clearLogs();
        m_state.setBanner("Terminal de logs limpo.", false);
        screen.PostEvent(Event::Custom);
    }, dangerButtonStyle());

    Component logsContainer = Container::Horizontal({logFilterMenu, clearLogsBtn});

    Component logsView = Renderer(logsContainer, [&]() {
        const TuiSnapshot snap = m_state.snapshot();
        const std::vector<std::string> filtered = TuiState::filterLogs(snap.logs, selectedLogFilter);

        Elements logLines;
        if (filtered.empty()) {
            logLines.push_back(text("  Nenhum log registrado para este filtro.") | color(kSlateMuted));
        } else {
            const std::size_t start = filtered.size() > 60 ? filtered.size() - 60 : 0;
            for (std::size_t i = start; i < filtered.size(); ++i) {
                const std::string &line = filtered.at(i);
                Color lineColor = kEmeraldBright;
                if (line.find("Erro") != std::string::npos || line.find("ERROR") != std::string::npos ||
                    line.find("Falha") != std::string::npos) {
                    lineColor = kRed;
                } else if (line.find("Aviso") != std::string::npos || line.find("WARNING") != std::string::npos) {
                    lineColor = kAmber;
                }
                logLines.push_back(text(" " + line) | color(lineColor));
            }
        }

        return vbox({
            hbox({
                text(" Filtro: ") | bold | color(kSlateMuted) | center,
                logFilterMenu->Render() | center,
                filler(),
                text("Total: " + std::to_string(snap.logs.size()) + " | Visiveis: " + std::to_string(filtered.size()) + "  ")
                    | color(kSlateMuted) | center,
                clearLogsBtn->Render(),
            }) | borderStyled(ROUNDED, kBorderColor),
            vbox({
                text(" TERMINAL DE LOGS ") | bold | color(kEmeraldBright),
                separator() | color(kBorderColor),
                vbox(std::move(logLines)) | yframe | flex,
            }) | borderStyled(ROUNDED, kBorderColor) | flex,
        });
    });

    // --- Tab 4: Sistema ---
    Component refreshDiagBtn = Button(" Atualizar Diagnostico ", [&]() {
        m_backend.refreshSystemInfo();
    }, primaryButtonStyle());

    Component systemView = Renderer(refreshDiagBtn, [&]() {
        const TuiSnapshot snap = m_state.snapshot();
        const TuiSystemInfo &sys = snap.system;

        auto kvRow = [](const std::string &key, const std::string &val, Color valColor) {
            return hbox({
                text(" " + key) | bold | color(kSlateMuted) | size(WIDTH, EQUAL, 24),
                text(val) | bold | color(valColor),
            });
        };

        return vbox({
            vbox({
                text(" APLICATIVO E AMBIENTE ") | bold | color(kEmeraldBright),
                separator() | color(kBorderColor),
                kvRow("Aplicativo:", "Prism Downloader (Studio TUI)", kSlateText),
                kvRow("Versao:", m_versionTag, kEmeraldBright),
                kvRow("Arquitetura:", "C++20 + Qt6 Core + FTXUI", kSlateText),
                kvRow("Pasta de Destino:", snap.outputFolder, kCyan),
            }) | borderStyled(ROUNDED, kBorderColor),

            vbox({
                text(" HARDWARE E MOTORES ") | bold | color(kEmeraldBright),
                separator() | color(kBorderColor),
                kvRow("GPU Detectada:", sys.gpuName, sys.hardwareAcceleration ? kEmeraldBright : kAmber),
                kvRow("Codec Recomendado:", sys.gpuCodec, kCyan),
                kvRow("yt-dlp Versao:", sys.ytdlpVersion + " (" + sys.ytdlpSource + ")",
                      sys.ytdlpPath.empty() ? kRed : kEmeraldBright),
                kvRow("yt-dlp Caminho:", sys.ytdlpPath.empty() ? "Nao encontrado" : sys.ytdlpPath, kSlateMuted),
                kvRow("FFmpeg Origem:", sys.ffmpegSource,
                      sys.ffmpegPath.empty() ? kRed : kEmeraldBright),
                kvRow("FFmpeg Caminho:", sys.ffmpegPath.empty() ? "Nao encontrado" : sys.ffmpegPath, kSlateMuted),
                separatorEmpty(),
                hbox({text(" "), refreshDiagBtn->Render()}),
            }) | borderStyled(ROUNDED, kBorderColor),
            filler(),
        });
    });

    Component mainTabContainer = Container::Tab({
        downloadsView,
        libraryView,
        converterView,
        logsView,
        systemView,
    }, &activeTab);

    Component sidebarContainer = Container::Vertical({navMenu, quitBtn});
    Component rootContainer = Container::Horizontal({sidebarContainer, mainTabContainer});

    Component appRenderer = Renderer(rootContainer, [&]() {
        const TuiSnapshot snap = m_state.snapshot();

        Element header = hbox({
            text(" PRISM DOWNLOADER ") | bold | color(Color::RGB(4, 24, 16)) | bgcolor(kEmerald),
            text(" STUDIO TUI " + m_versionTag) | bold | color(kEmeraldBright),
            filler(),
            text("Fila: " + std::to_string(snap.activeDownloads) + " ativo(s), " +
                 std::to_string(snap.pendingDownloads) + " aguardando  |  ") | color(kSlateText),
            text(truncateText(snap.system.gpuName, 28)) | color(kCyan),
            text(" "),
        }) | borderStyled(LIGHT, kBorderColor);

        Element sidebar = vbox({
            text(" NAVEGACAO ") | bold | color(kEmeraldBright),
            separator() | color(kBorderColor),
            navMenu->Render(),
            filler(),
            separator() | color(kBorderColor),
            text(" F1-F5: Telas") | color(kSlateMuted),
            text(" Tab: Foco") | color(kSlateMuted),
            text(" Ctrl+Q: Sair") | color(kSlateMuted),
            separatorEmpty(),
            quitBtn->Render() | center,
        }) | borderStyled(ROUNDED, kBorderColor) | size(WIDTH, EQUAL, 24);

        Element footer = hbox({
            text(" STATUS: ") | bold | color(snap.statusIsError ? kRed : kEmeraldBright),
            text(snap.statusBanner) | color(snap.statusIsError ? kRed : kSlateText),
            filler(),
            text(" Tab: Alternar foco | Setas: Navegar | Enter: Confirmar ") | color(kSlateMuted),
        }) | borderStyled(LIGHT, kBorderColor);

        return vbox({
            header,
            hbox({
                sidebar,
                text(" "),
                mainTabContainer->Render() | flex,
            }) | flex,
            footer,
        }) | bgcolor(Color::RGB(11, 15, 20));
    });

    Component appWithShortcuts = CatchEvent(appRenderer, [&](Event event) {
        if (event == Event::Character('\x11')) { // Ctrl+Q
            screen.ExitLoopClosure()();
            return true;
        }
        if (event == Event::F1) {
            activeTab = 0;
            return true;
        }
        if (event == Event::F2) {
            activeTab = 1;
            m_backend.refreshLibrary(outputDirText);
            return true;
        }
        if (event == Event::F3) {
            activeTab = 2;
            return true;
        }
        if (event == Event::F4) {
            activeTab = 3;
            return true;
        }
        if (event == Event::F5) {
            activeTab = 4;
            return true;
        }
        return false;
    });

    screen.Loop(appWithShortcuts);
    m_backend.setStateChangedCallback({});
    return 0;
}
