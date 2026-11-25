# 📦 Guia de Compilação e Empacotamento — Prism Downloader

<p align="center">
  <a href="../BUILD_PACKAGING.md">🇺🇸 Read the English version here.</a>
</p>

Este guia fornece instruções detalhadas para configurar o ambiente de desenvolvimento, compilar o código fonte em **C++17**, executar os testes unitários e gerar os pacotes oficiais de distribuição para **Windows** e **Linux**.

---

## 💻 1. Ambiente Windows (10 / 11 64-bit)

### 1.1. Pré-requisitos
* **Compilador C++:** Microsoft Visual C++ (MSVC) 2019 ou 2022 (instalado via *Visual Studio Community* com carga de trabalho "Desenvolvimento para Desktop com C++").
* **CMake:** Versão 3.16 ou superior.
* **Qt 6:** Versão 6.7.x ou superior (componentes `Widgets` e `Network`, ex: em `C:/Qt/6.7.2/msvc2019_64`).
* **Inno Setup 6:** Necessário para compilar o instalador oficial (`setup_script.iss`).

### 1.2. Configuração e Compilação com CMake
Abra o prompt de comando do desenvolvedor (*x64 Native Tools Command Prompt for VS*) ou PowerShell:

```powershell
# Configurar o diretório de build
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
      -DCMAKE_BUILD_TYPE=Release `
      -DPRISM_QT_ROOT="C:/Qt/6.7.2/msvc2019_64" `
      -DBUILD_TESTING=ON

# Compilar o binário principal e auxiliares em modo Release
cmake --build build --config Release --parallel
```

### 1.3. Executando a Suíte de Testes Unitários
```powershell
ctest --test-dir build -C Release --output-on-failure
```

### 1.4. Geração de Pacotes de Distribuição (Windows)

#### Pacote Portátil (`.zip`)
Compacte a pasta com o executável compilado (`build/Release/`), as DLLs de runtime do Qt 6 (`Qt6Core.dll`, `Qt6Gui.dll`, `Qt6Widgets.dll`, `Qt6Network.dll`), `app_icon.ico`, `portable-update-helper.exe` e os binários de mídia (`ffmpeg.exe` e `yt-dlp.exe`).

#### Instalador Oficial (`.exe`)
Utilize o script Inno Setup para compilar o instalador:
```powershell
& "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe" setup_script.iss
```
O arquivo resultante será gravado em `dist/PrismDownloader_vX.Y.Z_Setup.exe`.

---

## 🐧 2. Ambiente Linux (Mint 22 / Ubuntu 24.04 amd64)

### 2.1. Instalação das Dependências do Sistema
No terminal do Linux, instale as ferramentas de compilação, Qt 6 e FFmpeg:

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-base-dev-tools \
                 ffmpeg dpkg-dev
```

### 2.2. Configuração e Compilação
```bash
# Configurar o projeto com CMake
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

# Compilar em paralelo
cmake --build build --parallel
```

### 2.3. Executando os Testes
```bash
ctest --test-dir build --output-on-failure
```

### 2.4. Geração do Pacote Nativo Debian (`.deb`)
O Prism Downloader utiliza o **CPack** para empacotar o executável, atalhos de desktop, ícones de alta resolução e dependências do sistema:

```bash
# Gerar o pacote .deb na pasta package/
cpack --config build/CPackConfig.cmake -G DEB -B package

# Instalar e testar o pacote gerado localmente
sudo apt install ./package/prism-downloader_*.deb
```

### 2.5. Outros formatos Linux
O formato de um build é escolhido com `-DPRISM_LINUX_PACKAGE=<id>` (define qual asset da release o atualizador do app baixa):

| Id | Saída | Observações |
| :--- | :--- | :--- |
| `deb-ubuntu24.04` (padrão) | `cpack -G DEB` | Ubuntu 24.04 / Mint 22 (nome histórico do asset) |
| `deb-ubuntu22.04`, `deb-debian12` | `cpack -G DEB` | Compile na própria distribuição; o CI renomeia o asset com sufixo |
| `rpm` | `cpack -G RPM` | Compile no Fedora; requer `rpm-build`. Depende de `/usr/bin/ffmpeg` |
| `bundle` | AppImage + tar.gz | Qt 6.7.2 com `linuxdeploy` + `linuxdeploy-plugin-qt` (fixados por SHA-256 no `ci.yml`); faça `cmake --install` num AppDir antes |

---

## 🧪 3. Suíte de Testes Automatizados (`tests/`)

O projeto inclui uma bateria completa de testes unitários integrados ao CTest:

| Teste | Arquivo | Responsabilidade |
| :--- | :--- | :--- |
| `QueueManagerTests` | `tests/QueueManagerTests.cpp` | Valida enfileiramento, limites de concorrência (1 a 5), cancelamentos atômicos e estados da fila. |
| `AppUpdateServiceTests` | `tests/AppUpdateServiceTests.cpp` | Testa parsing do manifesto, assets obrigatórios e cálculo de hash SHA-256. |
| `YtDlpUpdateServiceTests` | `tests/YtDlpUpdateServiceTests.cpp` | Valida parsing de releases do yt-dlp e verificação de integridade de checksums. |
| `MediaToolResolverTests` | `tests/MediaToolResolverTests.cpp` | Testa resolução de prioridade de binários (User Update vs. Bundle vs. PATH) e comparação de versões. |
| `PortableUpdateCommonTests` | `tests/PortableUpdateCommonTests.cpp` | Valida comandos de extração e argumentos do assistente portátil. |
| `DownloadProfileTests` | `tests/DownloadProfileTests.cpp` | Valida seletores de formato de qualidade gerados para o yt-dlp. |
| `TuiStateTests` | `tests/TuiStateTests.cpp` | Valida transições de estado da TUI interativa, filtros de log, varredura de biblioteca e formatação. |

### 3.1. Interface Interativa no Terminal (`prism-cli`)
Junto com o executável gráfico, a compilação gera o `prism-cli` (`prism-cli.exe` no Windows e `/usr/bin/prism-cli` no Linux), uma interface interativa de terminal (TUI) construída com **FTXUI** (baixado e validado por SHA-256 via `cmake/FtxuiDependency.cmake`). Funciona nativamente no Windows Terminal/PowerShell/CMD e em terminais Linux (incluindo sessões SSH sem ambiente gráfico), oferecendo navegação interativa para Downloads, Biblioteca, Conversor, Logs e Diagnóstico do Sistema.

---

## 🌐 4. Pipeline de Integração Contínua (CI/CD)

O repositório possui dois workflows do GitHub Actions:
* `.github/workflows/ci.yml` (manual: Actions > CI build > Run workflow) compila, testa e empacota Linux/Windows sem publicar nada.
* `.github/workflows/release.yml` pode ser executado manualmente pelo GitHub Actions (Actions > Release > Run workflow, com opções de `tag`, `draft` e `prerelease`) ou disparado quando uma tag `v*` é enviada (ex.: `git tag v2.0.3 && git push origin v2.0.3`). A tag precisa ser igual a `v<PRISM_VERSION>` em `version.iss` (na execução manual com `tag` vazio, usa `v<PRISM_VERSION>` automaticamente). Ele reutiliza o `ci.yml` e publica a release, usando `changelog/<versão>.md` como notas quando existir.

O pipeline:
1. Constrói os pacotes Linux: `.deb` para Ubuntu 24.04, Ubuntu 22.04 e Debian 12; `.rpm` em container Fedora; AppImage e tar.gz (Qt embutido) no Ubuntu 22.04.
2. Compila a suíte de testes e executa o `ctest`.
3. Gera `prism-update-manifest.json` com os hashes SHA-256 dos artefatos da release.
4. Publica o manifesto e os artefatos diretamente na aba de Releases do GitHub.
