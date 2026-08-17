# 📦 Guia de Compilação e Empacotamento — Prism Downloader

Este guia fornece instruções detalhadas para configurar o ambiente de desenvolvimento, compilar o código fonte em **C++17**, executar os testes unitários e gerar os pacotes oficiais de distribuição para **Windows** e **Linux**.

---

## 💻 1. Ambiente Windows (10 / 11 64-bit)

### 1.1. Pré-requisitos
* **Compilador C++:** Microsoft Visual C++ (MSVC) 2019 ou 2022 (instalado via *Visual Studio Community* com carga de trabalho "Desenvolvimento para Desktop com C++").
* **CMake:** Versão 3.16 ou superior.
* **Qt 6:** Versão 6.7.x ou superior (componentes `Widgets` e `Network`, ex: em `C:/Qt/6.7.2/msvc2019_64`).
* **OpenSSL:** Versão 1.1.1 ou 3.x (incluindo headers e `libcrypto`).
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
& "C:\Users\Admin\AppData\Local\Programs\Inno Setup 6\ISCC.exe" setup_script.iss
```
O arquivo resultante será gravado em `dist/PrismDownloader_vX.Y.Z_Setup.exe`.

---

## 🐧 2. Ambiente Linux (Mint 22 / Ubuntu 24.04 amd64)

### 2.1. Instalação das Dependências do Sistema
No terminal do Linux, instale as ferramentas de compilação, Qt 6 e OpenSSL:

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-base-dev-tools \
                 libssl-dev ffmpeg dpkg-dev
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

---

## 🧪 3. Suíte de Testes Automatizados (`tests/`)

O projeto inclui uma bateria completa de testes unitários integrados ao CTest:

| Teste | Arquivo | Responsabilidade |
| :--- | :--- | :--- |
| `QueueManagerTests` | `tests/QueueManagerTests.cpp` | Valida enfileiramento, limites de concorrência (1 a 5), cancelamentos atômicos e estados da fila. |
| `AppUpdateServiceTests` | `tests/AppUpdateServiceTests.cpp` | Testa verificação de manifestos, cálculo de hash SHA-256 e validação de assinaturas Ed25519. |
| `YtDlpUpdateServiceTests` | `tests/YtDlpUpdateServiceTests.cpp` | Valida parsing de releases do yt-dlp e verificação de integridade de checksums. |
| `MediaToolResolverTests` | `tests/MediaToolResolverTests.cpp` | Testa resolução de prioridade de binários (User Update vs. Bundle vs. PATH) e comparação de versões. |
| `PortableUpdateCommonTests` | `tests/PortableUpdateCommonTests.cpp` | Valida comandos de extração e argumentos do assistente portátil. |
| `DownloadProfileTests` | `tests/DownloadProfileTests.cpp` | Valida seletores de formato de qualidade gerados para o yt-dlp. |

---

## 🌐 4. Pipeline de Integração Contínua (CI/CD)

O repositório possui uma action automatizada no GitHub (`.github/workflows/release-linux.yml`) que:
1. Constrói o pacote Linux `.deb` em ambiente limpo Ubuntu 24.04.
2. Compila a suíte de testes e executa o `ctest`.
3. Assina o manifesto de release `prism-update-manifest.json` com a chave privada Ed25519 armazenada nos segredos do repositório.
4. Publica os artefatos diretamente na aba de Releases do GitHub.
