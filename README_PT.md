<p align="center">
  <img src="app_icon.png" width="140" alt="Prism Downloader Icon Logo" style="border-radius: 24px; box-shadow: 0px 4px 20px rgba(0,255,150,0.2);"/>
</p>

<h1 align="center">💎 Prism Downloader — Suíte Tonho Studios</h1>

<p align="center">
  <b>A suíte desktop mais completa, rápida e inteligente para download, recorte e conversão de mídias.</b><br>
  Projetada com alta precisão em <b>C++17 Puro</b> e <b>Qt 6</b>, com aceleração nativa por placa de vídeo (GPU) e logs de telemetria avançados.
</p>

<p align="center">
  <a href="README.md">🇺🇸 Read the English version here.</a>
</p>

<p align="center">
  <a href="https://github.com/BadTonho/PrismDownloader/releases/latest"><img src="https://img.shields.io/github/v/release/BadTonho/PrismDownloader?color=00e676&label=Release%20Oficial&style=for-the-badge&logo=github" alt="GitHub Release"></a>
  <a href="https://isocpp.org/"><img src="https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white" alt="C++17"></a>
  <a href="https://www.qt.io/"><img src="https://img.shields.io/badge/Qt_GUI-6.7%20Dark%20Tech-41CD52?style=for-the-badge&logo=qt&logoColor=white" alt="Qt 6"></a>
  <a href="https://developer.nvidia.com/video-codec-sdk"><img src="https://img.shields.io/badge/NVIDIA-NVENC%20Hardware-76B900?style=for-the-badge&logo=nvidia&logoColor=white" alt="NVIDIA NVENC"></a>
  <img src="https://img.shields.io/badge/Plataforma-Windows%20%7C%20Linux-00a859?style=for-the-badge&logo=linux&logoColor=white" alt="Windows e Linux">
  <a href="LICENSE"><img src="https://img.shields.io/badge/Licen%C3%A7a-MIT-6b21a8?style=for-the-badge&logo=open-source-initiative&logoColor=white" alt="Licença MIT"></a>
</p>

---

## ⚡ O que é o Prism Downloader?

Desenvolvido pela **Tonho Studios**, o **Prism Downloader** oferece um fluxo de trabalho limpo para desktop para download e processamento de áudio e vídeo. Ele combina alto desempenho em **C++17**, **Qt 6**, e os motores de referência da indústria `yt-dlp` e `FFmpeg` no Windows e no Linux.

---

## 🔥 Principais Diferenciais e Recursos

### 🚦 Fila de Sessões Concorrentes
* Adicione quantos links desejar e execute de **1 a 5 downloads simultâneos** (padrão: 2), com monitoramento isolado de progresso, velocidade, ETA, status e cancelamento por tarefa.
* Conversões manuais e automáticas compartilham uma fila FIFO estrita com exatamente um processo FFmpeg ativo por vez, evitando sobrecarga na GPU ou CPU.

### 🎬 1. Recorte Inteligente de Faixa de Tempo (*Time-Slice Trimming*)
* **Economize banda baixando apenas o trecho que você precisa!**
* Defina o tempo de início e término diretamente no painel (ex: das `00:03:15` até `00:05:45`). O motor solicita e descarrega apenas o trecho desejado em tempo recorde.

### ⚡ 2. Aceleração Gráfica de Baixo Nível (NVENC, VAAPI, AMF e QSV)
* **Aceleração por Hardware com Fallback Seguro:** O Windows utiliza detecção nativa de adaptadores; o Linux testa os encoders expostos pelo FFmpeg e o dispositivo DRM (`/dev/dri`). NVIDIA NVENC, AMD/Intel VAAPI, AMD AMF e Intel QSV são acionados apenas quando inicializam com sucesso; a CPU permanece como fallback confiável.
* O codec selecionado é exibido na tela de conversão. Em sistemas AMD no Linux, o backend comum é `h264_vaapi`/`hevc_vaapi`, com recuo automático para CPU se permissões ou drivers não estiverem disponíveis.
* Para diagnosticar a instalação no Linux sem abrir a interface, execute `prism-downloader --diagnose-gpu`.

### 📡 3. Terminal de Logs em Tempo Real com Filtros
* Acompanhe cada etapa de download, conversão e extração de áudio através de um terminal integrado na aba dedicada da aplicação.
* **Barra de Filtros Dinâmicos:** Alterne instantaneamente entre:
  * 🌐 **Todos os Logs:** Console completo.
  * ⚙️ **Processos:** Saída bruta de comandos e telemetria de subprocessos.
  * ❌ **Erros:** Isolamento visual em vermelho de códigos de erro e alertas.
  * 📌 **Sistema e Geral:** Notificações de auto-atualização, detecção de hardware e status do motor.
  * 🧹 **Limpar Terminal:** Limpa o buffer de exibição da tela.

### 🛡️ 4. Gestão Gráfica de Processos
* Nenhuma janela de prompt de comando preta é aberta durante a execução dos downloads.
* O motor utiliza **`QProcess::start(program, arguments)`** com monitoramento estrito de códigos de saída. No Linux, cada download possui sua própria sessão de processo para que o cancelamento elimine também os processos filhos do FFmpeg.

### ☁️ 5. Central de Atualizações Tonho Studios
* **Sincronização com GitHub Releases:** Verificação assíncrona de novas versões na inicialização com suporte a checagem em segundo plano.
* **Auto-atualização segura para Windows e Linux:** Cada release publica um manifesto assinado com criptografia Ed25519 contendo os hashes SHA-256 dos pacotes Setup, Portátil e DEB. O Prism rejeita atualizações sem assinatura válida ou adulteradas.
* **Motor yt-dlp atualizável autonomamente:** O aplicativo traz o canal Nightly do `yt-dlp` e permite atualizá-lo com validação SHA-256 diretamente na pasta do usuário, sem necessidade de permissões de administrador.

---

## 📦 Instalação e Download

Acesse a [Página Oficial de Releases no GitHub](https://github.com/BadTonho/PrismDownloader/releases/latest) para baixar o pacote ideal para o seu sistema operacional:

| Pacote | Finalidade |
| :--- | :--- |
| ⭐ **[`PrismDownloader_vX.X.X_Setup.exe`](https://github.com/BadTonho/PrismDownloader/releases/latest)** | **Instalador Oficial Tonho Studios** (Recomendado). Assistente completo em Português (Brasil) com criação de atalho na Área de Trabalho. |
| 💼 **[`PrismDownloader_vX.X.X_Portable.zip`](https://github.com/BadTonho/PrismDownloader/releases/latest)** | **Arquivo Portátil Autônomo**. Extraia em qualquer pasta ou pendrive e execute imediatamente. |
| 🐧 **[`prism-downloader_X.Y.Z_amd64.deb`](https://github.com/BadTonho/PrismDownloader/releases/latest)** | Pacote para **Linux Mint 22 / Ubuntu 24.04 amd64**. Instale com `sudo apt install ./prism-downloader_X.Y.Z_amd64.deb`. |

---

## 🛠️ Compilação a partir do Código Fonte (Para Desenvolvedores)

### Linux Mint 22 / Ubuntu 24.04 (amd64)

Instale os requisitos de desenvolvimento e compile:

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev qt6-base-dev-tools libssl-dev ffmpeg dpkg-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
cpack --config build/CPackConfig.cmake -G DEB -B package
sudo apt install ./package/prism-downloader_*.deb
```

### Windows 10 / 11 (64-bit)

Para compilar nativamente no Windows:

#### Requisitos do Sistema
* **Windows 10 ou 11 (64-bit)**
* **Visual Studio 2019 / 2022** (com suporte ao compilador MSVC C++ x64)
* **Qt 6.7+** (componentes `Widgets` e `Network`)
* **OpenSSL 1.1.1+** (headers e bibliotecas de desenvolvimento `libcrypto`)
* **CMake 3.16+**
* **Inno Setup 6+** (opcional, para compilar o instalador `.exe`)

#### Passos Rápidos para Build via CMake (PowerShell)
```powershell
# 1. Clone o repositório oficial
git clone https://github.com/BadTonho/PrismDownloader.git
cd PrismDownloader

# 2. Gere os arquivos de projeto apontando para o Visual Studio
cmake -S . -B build -G "Visual Studio 17 2022" -A x64

# 3. Compile em modo Release
cmake --build build --config Release

# 4. Injete as dependências do Qt e execute
C:\Qt\6.7.2\msvc2019_64\bin\windeployqt.exe --release --no-translations .\build\Release\PrismDownloader.exe
.\build\Release\PrismDownloader.exe
```

---

## 📚 Central de Documentação Técnica

Para detalhes aprofundados sobre arquitetura, especificação de módulos, aceleração por GPU e testes unitários, consulte a [**Central de Documentação**](docs/pt/README.md) (ou a [versão principal em Inglês](docs/README.md)):

* 🏗️ [**Arquitetura do Sistema & Concorrência**](docs/pt/ARQUITETURA.md) — Fluxo de dados, concorrência e isolamento de processos.
* 🧩 [**Referência de Módulos & Código**](docs/pt/MODULOS.md) — Classes, métodos públicos, sinais/slots e structs.
* ⚡ [**Aceleração por Hardware & GPU**](docs/pt/HARDWARE_GPU.md) — Matriz de hardware, sondagem e diagnóstico por CLI.
* 🛡️ [**Sistema de Atualização Segura**](docs/pt/AUTO_UPDATE.md) — Criptografia Ed25519, hashes SHA-256 e auto-update.
* 🎨 [**Guia de Interface (UI/UX)**](docs/pt/INTERFACE_UI.md) — Interface Dark Tech, playlists, biblioteca e telemetria de logs.
* 📦 [**Guia de Compilação & Empacotamento**](docs/pt/BUILD_PACKAGING.md) — Pré-requisitos, testes CTest e criação de pacotes.

---

## ⚖️ Licença e Direitos Autorais

Este software é disponibilizado publicamente sob os termos da **Licença MIT** (consulte o arquivo [LICENSE](LICENSE)).  
A comunidade possui liberdade legal para utilizar, auditar e redistribuir esta ferramenta, **desde que preservada a nota de direitos autorais (Copyright © Tonho Studios)**.

* Para relatar falhas críticas ou vulnerabilidades, consulte a nossa [Política de Segurança](SECURITY.md).
* Deseja colaborar com o código C++ do projeto? Leia o nosso [Guia de Contribuição](CONTRIBUTING.md).

---

<p align="center">
  <b>Desenvolvido com ☕ e extrema dedicação por <a href="https://github.com/BadTonho">Tonho Studios</a></b><br>
  <i>Todos os direitos reservados • Engenharia em C++ e Performance Visual para Desktop.</i>
</p>
