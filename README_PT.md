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
  <a href="https://www.microsoft.com/windows"><img src="https://img.shields.io/badge/Platform-Windows%2010%20%7C%2011-0078D6?style=for-the-badge&logo=windows&logoColor=white" alt="Windows"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-6b21a8?style=for-the-badge&logo=open-source-initiative&logoColor=white" alt="MIT License"></a>
</p>

---

## ⚡ O que é o Prism Downloader?

Desenvolvido pela **Tonho Studios**, o **Prism Downloader** nasceu com o objetivo de revolucionar a experiência de download e processamento de áudio e vídeo em desktops Windows. Como um prisma óptico que deforma e organiza feixes de dados, a suíte combina o poder da linguagem de programação de alta performance **C++17** com motores consagrados na indústria (`yt-dlp` e `FFmpeg`) executados diretamente na API Win32 do Windows para oferecer um fluxo ultrarápido, limpo, seguro e transparente.

---

## 🔥 Funcionalidades de Destaque

### 🚦 Fila simultânea da sessão
* Adicione quantas URLs precisar e execute **de 1 a 5 downloads ao mesmo tempo** (padrão: 2), com progresso, velocidade, ETA, estado e cancelamento isolados por tarefa.
* Conversões automáticas e manuais compartilham uma fila FIFO com exatamente um FFmpeg ativo, evitando sobrecarga da GPU/CPU.

### 🎬 1. Recorte de Faixa de Tempo (Time-Slice Extraction)
* **Economize banda baixando trechos específicos!** 
* Defina o intervalo exato de corte diretamente no painel (ex: das `00:03:15` às `00:05:45`). O motor processa e baixa exclusivamente os bytes necessários da nuvem em tempo recorde!

### ⚡ 2. Aceleração Gráfica de Baixo Nível (NVIDIA NVENC & AMD)
* **Detecção via RAM (0.001ms):** O sistema interroga a API nativa Win32 (`EnumDisplayDevicesA`) no segundo em que o app inicia para identificar processadores gráficos dedicados (compatível com **NVIDIA GeForce GTX 1660 SUPER** e superiores, além de **AMD Radeon AMF**).
* Todas as conversões de formatos utilizam codecs de hardware (`h264_nvenc`), poupando 100% da sua CPU e reduzindo tempos de renderização a meros segundos.

### 📡 3. Terminal de Logs em Tempo Real com Filtros de Telemetria
* Acompanhe cada etapa de download, conversão e extração de áudio através de um terminal integrado na aba dedicada do aplicativo.
* **Barra de Filtros Dinâmica:** Alterne em tempo real entre:
  * 🌐 **Todos os Logs:** Fluxo integral do console.
  * ⚙️ **Apenas Processos:** Saída bruta dos comandos e processos Windows.
  * ❌ **Apenas Erros:** Isolamento visual de erros e alertas destacados em vermelho.
  * 📌 **Sistema & Gerais:** Notificações do Auto-updater, detecções de hardware e status de rotina.
  * 🧹 **Limpar Terminal:** Limpeza instantânea do buffer de exibição.

### 🛡️ 4. Sandboxing Gráfico e Zero Janelas de Terminal (`CREATE_NO_WINDOW`)
* Esqueça aplicativos que ficam abrindo ou piscando janelas pretas do *CMD* ou *Windows Terminal* durante downloads!
* O motor executa os binários via **`QProcess::start(program, arguments)`** usando travas do Kernel Win32 (`0x08000000`), garantindo uma operação 100% gráfica, silenciosa e com monitoramento nativo de `exit code` para evitar falsos positivos de conclusão.

### ☁️ 5. Central de Atualizações Tonho Studios
* **Sincronia Global com o GitHub:** Checagem de versões assíncrona baseada na nuvem com suporte a modo silencioso de inicialização. 
* **Links seguros de atualização:** Abre a página oficial do Prism Downloader ou do `yt-dlp` para atualização manual verificada pelo usuário; o aplicativo não baixa nem executa instaladores automaticamente.

---

## 📦 Como Instalar e Usar

Acesse a nossa aba de [Releases Oficiais no GitHub](https://github.com/BadTonho/PrismDownloader/releases/latest) para baixar os pacotes completos prontos para uso no Windows (já acompanham motores FFmpeg e yt-dlp embutidos):

| Pacote Disponível | O que oferece? |
| :--- | :--- |
| ⭐ **[`PrismDownloader_vX.X.X_Setup.exe`](https://github.com/BadTonho/PrismDownloader/releases/latest)** | **Instalador Oficial da Tonho Studios** (Recomendado). Assistente de instalação completo em Português do Brasil com criação de atalho na Área de Trabalho. |
| 💼 **[`PrismDownloader_vX.X.X_Portable.zip`](https://github.com/BadTonho/PrismDownloader/releases/latest)** | **Pacote Portável sem Instalação.** Basta extrair a pasta no seu HD ou Pen Drive e executar diretamente. |

---

## 🛠️ Como Compilar o Projeto do Zero (Para Desenvolvedores)

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
  <i>Todos os direitos reservados • Engenharia em C++ e Performance Visual no Windows.</i>
</p>
