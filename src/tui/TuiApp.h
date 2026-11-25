#ifndef PRISM_TUI_APP_H
#define PRISM_TUI_APP_H

#include <string>

class TuiBackend;
class TuiState;

class TuiApp final {
public:
    TuiApp(TuiState &state, TuiBackend &backend, std::string versionTag);

    int run();

private:
    TuiState &m_state;
    TuiBackend &m_backend;
    std::string m_versionTag;
};

#endif // PRISM_TUI_APP_H
