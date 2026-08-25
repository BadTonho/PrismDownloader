# 🗺️ Roadmap e estado atual — Prism Downloader

Este documento registra o escopo entregue e as próximas melhorias do Prism Downloader, um aplicativo desktop de download e processamento de mídia focado em **alta velocidade**, **baixo uso de CPU** e **suporte multiplataforma de GPU**.

---

## 🚀 1. Visão Geral do Projeto (Overview)

* **Objetivo:** Oferecer um aplicativo desktop moderno, inteligente e ágil, capaz de lidar com vídeos longos (cursos, lives e podcasts) sem travamentos ou lentidão desnecessária.
* **Diferenciais Chave:**
  1. **Download + Muxing Instantâneo (*Stream Copy*):** Priorizar a união de faixas originais de áudio e vídeo sem recodificar (takes de segundos, 0% de sobrecarga).
  2. **Detecção Inteligente de Hardware:** Quando a conversão for necessária (ex: trocar formato, comprimir ou converter para áudio), detectar automaticamente a placa de vídeo do usuário e usar aceleração dedicada.
  3. **Pronto para Distribuição:** Distribuir os motores necessários (`FFmpeg` e `yt-dlp`) nos pacotes suportados, com validação de integridade e instruções de instalação.

---

## 🧠 2. Estratégia de Hardware (GPU Multi-Marca)

O motor usa detecção automática de hardware e seleciona o codec apropriado quando a conversão exige recodificação:

| Hardware Detectado | Codec de Vídeo (H.264 / HEVC) | Vantagem |
| :--- | :--- | :--- |
| **NVIDIA** (Ex: GTX 1660 Super) | `h264_nvenc` / `hevc_nvenc` | Codificação ultrarrapida no chip dedicado NVENC. |
| **AMD Radeon** | `h264_amf` / `hevc_amf` | Aceleração nativa para placas RX/APUs Ryzen. |
| **Intel (Arc ou Integrada HD/Iris)** | `h264_qsv` / `hevc_qsv` | QuickSync da Intel, excelente para notebooks sem GPU dedicada. |
| **CPU Pura (Fallback)** | `libx264` (com Multithreading) | Uso otimizado de múltiplos núcleos caso o PC não possua GPU suportada. |

---

## 🛠️ 3. Etapas do Projeto (Desenvolvimento em Estágios)

### 📍 Etapa 1: Concepção & Decisões Arquiteturais (Concluída 🏁)
* [x] **1.1. Definição da Stack Principal & Visual:** **C++ com Qt Widgets** 🏆 (Escolhida)
  * **Por que essa escolha?** Combina a velocidade de inicialização e o baixo consumo do C++ com os componentes nativos do Qt 6, sem exigir interpretadores de terceiros.
  * Além disso, é nativa, fantástica para distribuição sem exigir instalação de interpretadores terceiros (como Python ou Node).
* [x] **1.2. Definição de Recursos MVP (Versão 1.0):** 🏁 (Concluído e Alinhado!)
  * ⚡ **Download Multiresolução:** Baixar vídeos em alta qualidade (4K, 2K, 1080p, 720p, 60fps) do YouTube e outras plataformas.
  * 🎵 **Extração de Áudio Direto:** Modo de conversão instantânea para MP3 ou FLAC em máxima qualidade (ideal para músicas, podcasts e áudio aulas).
  * 🏎️ **Motor "Zero-Lag" para Vídeos Longos (Stream Copy):** Evitar gargalos de processamento alavancando junção instantânea (*muxing*) sem sobrecarregar a CPU.
  * 🚀 **Aceleração por Hardware em GPU:** Acionar chips dedicados de vídeo nas conversões de arquivos (NVIDIA NVENC para a sua GTX 1660 Super, e detecção limpa para AMD/Intel).
  * 📚 **Suporte a Playlists e Vídeos Longos:** Gerenciador estável de fila, mostrando velocidade real (MB/s) e tempo restante.
  * ✂️ **Recorte Inteligente de Faixa de Tempo:** Poder recortar e baixar apenas um trecho específico de um vídeo ou live (Ex: das `10:15` às `18:40`) sem descarregar horas inteiras desnecessariamente.

---

### 📍 Etapa 2: Motor Core & Inteligência de Hardware em C++ 🏁 (Concluído!)
* [x] **2.1. Integração C++ <> Motores de Mídia:** Criados o [DownloadManager.cpp](src/DownloadManager.cpp), com fila assíncrona baseada em `QProcess` e concorrência configurável, e o [ConversionManager.cpp](src/ConversionManager.cpp), com conversões FIFO e fallback para CPU.
* [x] **2.2. Módulo de Detecção de GPU:** Criado o [GPUDetector.cpp](src/GPUDetector.cpp) em C++ nativo com sondagem de hardware e validação real dos encoders disponíveis no FFmpeg.
* [x] **2.3. Mídia Sem Perda (*Stream Copy*):** Estruturado o motor de download para união instantânea de faixas sem sobrecarga de processador.

---

### 📍 Etapa 3: Interface Gráfica e Experiência do Usuário (UI/UX) 🏁 (Concluído!)
* [x] **3.1. Design da Janela Principal:** Criada interface em Qt 6 com tema moderno (Dark Tech), campos de URL, qualidade e recorte de faixa de tempo em [MainWindow.cpp](src/MainWindow.cpp).
* [x] **3.2. Gerenciador de Downloads:** Implementada tabela por tarefa, concorrência de 1 a 5 downloads, progresso em tempo real, cancelamento individual/global e resumo único da sessão.
* [x] **3.3. Indicador Visual de Hardware:** Exibido na interface o encoder detectado e o estado de aceleração, com fallback explícito para CPU.

---

### 📍 Etapa 4: Diferenciais Inteligentes & Polimento do MVP 🏁 (Concluído!)
* [x] **4.1. Recorte Inteligente de Faixa de Tempo:** Implementado corte direto nas streams sem precisar processar arquivos inteiros em disco.
* [x] **4.2. Polimento de UI/UX e Tratamento de Erros:** Seletor automático para a pasta de Downloads, botão de ação ciano para abrir o Explorer diretamente nos arquivos salvos e alertas limpos ao usuário.
* [x] **4.3. Testes Reais do MVP:** Validada união instantânea em 1 segundo via Stream Copy no Windows e suporte a aceleração NVIDIA NVENC. Histórico e Gerenciador de Pastas: Abrir rapidamente a pasta com os arquivos já baixados.

---

### 📍 Etapa 5: Empacotamento e Preparação para Distribuição (Deploy) 🏁 (Concluído!)
* [x] **5.1. Bundle de Dependências:** Os pacotes suportados incluem o `yt-dlp` fixado e os runtimes necessários; o FFmpeg é empacotado no Windows e instalado como dependência no Debian.
* [x] **5.2. Pacote Portável Autônomo:** O pipeline gera um ZIP portátil com nome versionado `PrismDownloader_vX.Y.Z_Portable.zip`.
* [x] **5.3. Instalador Oficial:** O pipeline gera o instalador versionado `PrismDownloader_vX.Y.Z_Setup.exe` com Inno Setup 6.

---
*Status do Projeto: o MVP está implementado e possui testes automatizados, empacotamento Windows/Linux e atualização segura. As próximas tarefas são manutenção contínua, cobertura de testes e evolução modular da interface.*
