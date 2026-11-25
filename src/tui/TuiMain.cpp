#include "PrismVersion.h"
#include "TuiApp.h"
#include "TuiBackend.h"
#include "TuiState.h"

#include <iostream>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

void printHelp()
{
    std::cout
        << "Prism Downloader CLI / Studio TUI (" << PRISM_VERSION_TAG << ")\n"
        << "Uso:\n"
        << "  prism-cli               Inicia a interface interativa no terminal (TUI)\n"
        << "  prism-cli --version     Exibe a versao atual\n"
        << "  prism-cli --help        Exibe esta ajuda\n";
}

} // namespace

int main(int argc, char **argv)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--version" || arg == "-v") {
            std::cout << "prism-cli " << PRISM_VERSION_NUMBER << "\n";
            return 0;
        }
        if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        }
    }

    TuiState state;
    TuiBackend backend(state, PRISM_VERSION_NUMBER);
    backend.start(argc, argv);

    TuiApp app(state, backend, PRISM_VERSION_TAG);
    const int exitCode = app.run();

    backend.stop();
    return exitCode;
}
