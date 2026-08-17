<p align="center">
  <img src="app_icon.png" width="140" alt="Prism Downloader Icon Logo" style="border-radius: 24px; box-shadow: 0px 4px 20px rgba(0,255,150,0.2);"/>
</p>

<h1 align="center">💎 Prism Downloader — Tonho Studios Suite</h1>

<p align="center">
  <b>A mais completa, rápida e inteligente suíte desktop para download, recorte e conversão de mídias do mercado.</b><br>
  Construída com engenharia de alta precisão em <b>C++17 Puro</b> e <b>Qt 6</b>, com aceleração de hardware nativa e telemetria avançada de logs.
</p>

<p align="center">
  <a href="https://github.com/BadTonho/PrismDownloader/releases/latest"><img src="https://img.shields.io/github/v/release/BadTonho/PrismDownloader?color=00e676&label=Release%20Oficial&style=for-the-badge&logo=github" alt="GitHub Release"></a>
  <a href="https://isocpp.org/"><img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white" alt="C++17"></a>
  <a href="https://www.qt.io/"><img src="https://img.shields.io/badge/Qt_GUI-6.7%20Dark%20Tech-41CD52?style=for-the-badge&logo=qt&logoColor=white" alt="Qt 6"></a>
  <a href="https://developer.nvidia.com/video-codec-sdk"><img src="https://img.shields.io/badge/NVIDIA-NVENC%20Hardware-76B900?style=for-the-badge&logo=nvidia&logoColor=white" alt="NVIDIA NVENC"></a>
  <img src="https://img.shields.io/badge/Platform-Windows%20%7C%20Linux-00a859?style=for-the-badge&logo=linux&logoColor=white" alt="Windows e Linux">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-6b21a8?style=for-the-badge&logo=open-source-initiative&logoColor=white" alt="MIT License"></a>
</p>

---

## ⚡ O que é o Prism Downloader?

Desenvolvido pela **Tonho Studios**, o **Prism Downloader** oferece um fluxo limpo de desktop para download e processamento de áudio e vídeo. A suíte combina **C++17**, **Qt 6** e os motores consagrados `yt-dlp` e `FFmpeg` no Windows e no Linux.

---

## 🔥 Funcionalidades de Destaque

### 🚦 Fila simultânea da sessão
* Adicione quantas URLs precisar e execute **de 1 a 5 downloads ao mesmo tempo** (padrão: 2), com progresso, velocidade, ETA, estado e cancelamento isolados por tarefa.
* Conversões automáticas e manuais compartilham uma fila FIFO com exatamente um FFmpeg ativo, evitando sobrecarga da GPU/CPU.

### 🎬 1. Recorte de Faixa de Tempo (Time-Slice Extraction)
* **Economize banda baixando trechos específicos!** 
* Defina o intervalo exato de corte diretamente no painel (ex: das `00:03:15` às `00:05:45`). O motor processa e baixa exclusivamente os bytes necessários da nuvem em tempo recorde!

### ⚡ 2. Aceleração Gráfica de Baixo Nível (NVENC, VAAPI, AMF & QSV)
* **Aceleração de hardware com fallback seguro:** No Windows, a detecção é nativa; no Linux, o aplicativo testa os encoders disponibilizados pelo FFmpeg e o dispositivo DRM (`/dev/dri`). NVIDIA NVENC, AMD/Intel VAAPI, AMD AMF e Intel QSV são usados quando realmente inicializam; caso contrário, a conversão por CPU continua disponível.
* O codec selecionado é mostrado na tela de conversão. Em placas AMD no Linux, o backend normalmente é `h264_vaapi`/`hevc_vaapi`, enquanto o FFmpeg faz fallback para CPU se o driver ou a permissão do dispositivo não estiverem disponíveis.
* Para diagnosticar uma instalação Linux sem abrir a interface, execute `prism-downloader --diagnose-gpu`. O comando testa cada encoder, dispositivo DRM e mostra a saída completa do FFmpeg.

### 📡 3. Terminal de Logs em Tempo Real com Filtros de Telemetria
* Acompanhe cada etapa de download, conversão e extração de áudio através de um terminal integrado na aba dedicada do aplicativo.
* **Barra de Filtros Dinâmica:** Alterne em tempo real entre:
  * 🌐 **Todos os Logs:** Fluxo integral do console.
  * ⚙️ **Apenas Processos:** Saída bruta dos comandos e processos Windows.
  * ❌ **Apenas Erros:** Isolamento visual de erros e alertas destacados em vermelho.
  * 📌 **Sistema & Gerais:** Notificações do Auto-updater, detecções de hardware e status de rotina.
  * 🧹 **Limpar Terminal:** Limpeza instantânea do buffer de exibição.

### 🛡️ 4. Gerenciamento Gráfico de Processos
* Nenhuma janela de terminal é aberta durante os downloads.
* O motor usa **`QProcess::start(program, arguments)`** e monitora o código de saída. No Linux, cada download recebe uma sessão própria para que o cancelamento também finalize subprocessos do FFmpeg.

### ☁️ 5. Central de Atualizações Tonho Studios
* **Sincronia Global com o GitHub:** Checagem de versões assíncrona baseada na nuvem com suporte a modo silencioso de inicialização. 
* **Motor yt-dlp incluído e atualizável:** Os pacotes trazem uma Nightly oficial do `yt-dlp`. A central mostra a versão, a origem e a Nightly mais recente; o download só ocorre após confirmação e validação do checksum SHA-256.
* **Preferência pela versão mais nova:** Uma instalação manual mais recente no `PATH` também é reconhecida. O Prism nunca altera essa instalação: suas atualizações ficam apenas na pasta de dados do usuário.

---

## 📦 Como Instalar e Usar

Acesse a [página de Releases Oficiais no GitHub](https://github.com/BadTonho/PrismDownloader/releases/latest) para baixar o pacote da sua plataforma:

| Pacote Disponível | O que oferece? |
| :--- | :--- |
| ⭐ **[`PrismDownloader_vX.X.X_Setup.exe`](https://github.com/BadTonho/PrismDownloader/releases/latest)** | **Instalador Oficial da Tonho Studios** (Recomendado). Assistente de instalação completo em Português do Brasil com criação de atalho na Área de Trabalho. |
| 💼 **[`PrismDownloader_vX.X.X_Portable.zip`](https://github.com/BadTonho/PrismDownloader/releases/latest)** | **Pacote Portável sem Instalação.** Basta extrair a pasta no seu HD ou Pen Drive e executar diretamente. |
| 🐧 **[`prism-downloader_X.Y.Z_amd64.deb`](https://github.com/BadTonho/PrismDownloader/releases/latest)** | Pacote para **Linux Mint 22 / Ubuntu 24.04 amd64**. Instale com `sudo apt install ./prism-downloader_X.Y.Z_amd64.deb`. Inclui `yt-dlp` Nightly; o APT instala o FFmpeg. |

---

## 🛠️ Como Compilar o Projeto do Zero (Para Desenvolvedores)

### Linux Mint 22 / Ubuntu 24.04 (amd64)

Instale os requisitos de desenvolvimento e gere o pacote:

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-base-dev-tools ffmpeg dpkg-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cpack --config build/CPackConfig.cmake -G DEB -B package
sudo apt install ./package/prism-downloader_*.deb
```

O pacote é destinado inicialmente ao Mint 22 e Ubuntu 24.04 em `amd64`. Ele inclui o `yt-dlp_linux` Nightly oficial em `/usr/lib/prism-downloader/yt-dlp` e usa o `ffmpeg` do sistema. O CMake baixa a versão Nightly fixada e valida o SHA-256 durante a configuração; portanto, a criação do pacote requer acesso à internet. O aplicativo pode instalar uma Nightly nova sem `sudo` na pasta de dados do usuário e também escolhe uma cópia mais recente instalada manualmente no `PATH`.

### Windows 10 / 11 (64-bit)

Se você deseja compilar este projeto nativo em sua própria estação de trabalho Windows:

### Requisitos do Sistema
* **Windows 10 ou 11 (64-bit)**
* **Visual Studio 2019 / 2022** (com suporte ao compilador MSVC C++ x64)
* **Qt 6.7+** (componentes `Widgets` e `Network`)
* **CMake 3.16+**
* **Inno Setup 6+** (opcional, para compilar o instalador `.exe`)

### Passos Rápido para Build via CMake (PowerShell)
```powershell
# 1. Clone o repositório oficial
git clone https://github.com/BadTonho/PrismDownloader.git
cd PrismDownloader

# 2. Gere os arquivos de projeto apontando para o seu compilador Visual Studio
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# 3. Compile em modo Release
cmake --build build --config Release

# 4. Injete as dependências do Qt e execute
C:\Qt\6.7.2\msvc2019_64\bin\windeployqt.exe --release --no-translations .\build\Release\PrismDownloader.exe
.\build\Release\PrismDownloader.exe
```

---

## ⚖️ Licença e Direitos Autorais

Este software é disponibilizado publicamente sob os termos da **Licença MIT** (consulte o arquivo [LICENSE](LICENSE)).  
Isso significa que a comunidade possui liberdade legal para utilizar, auditar e redistribuir esta ferramenta, **desde que preservada a nota de direitos autorais (Copyright © Tonho Studios)**.

* Para relatar falhas críticas ou vulnerabilidades, consulte a nossa [Política de Segurança](SECURITY.md).
* Deseja colaborar com o código C++ do projeto? Leia o nosso [Guia de Contribuição](CONTRIBUTING.md).

---

<p align="center">
  <b>Desenvolvido com ☕ e extrema dedicação por <a href="https://github.com/BadTonho">Tonho Studios</a></b><br>
  <i>Todos os direitos reservados • Engenharia em C++ e Performance Visual para Desktop.</i>
</p>
