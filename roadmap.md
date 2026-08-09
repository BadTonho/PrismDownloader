# 🗺️ Roadmap - App de Download Rápido com Aceleração por GPU

Este documento descreve o planejamento e as etapas de desenvolvimento para a criação de um aplicativo desktop de download de vídeos (YouTube e outras plataformas), focado em **alta velocidade**, **baixo uso de CPU** e **suporte multiplataforma de placa de vídeo (GPU)** para futura distribuição.

---

## 🚀 1. Visão Geral do Projeto (Overview)

* **Objetivo:** Substituir ferramentas legadas e lentas por um aplicativo desktop moderno, inteligente e ágil, capaz de lidar com vídeos muito longos (cursos, lives, podcasts) sem trarações ou lentidão no computador.
* **Diferenciais Chave:**
  1. **Download + Muxing Instantâneo (*Stream Copy*):** Priorizar a união de faixas originais de áudio e vídeo sem recodificar (takes de segundos, 0% de sobrecarga).
  2. **Detecção Inteligente de Hardware:** Quando a conversão for necessária (ex: trocar formato, comprimir ou converter para áudio), detectar automaticamente a placa de vídeo do usuário e usar aceleração dedicada.
  3. **Pronto para Distribuição:** Embutir motores necessários (`FFmpeg` e `yt-dlp`) para que o usuário final apenas baixe e execute o `.exe`, sem configurar nada no sistema operativo.

---

## 🧠 2. Estratégia de Hardware (GPU Multi-Marca)

Como o app será distribuído para diversos usuários no futuro, o motor integrará uma **detecção automática de hardware**, usando o codec apropriado:

| Hardware Detectado | Codec de Vídeo (H.264 / HEVC) | Vantagem |
| :--- | :--- | :--- |
| **NVIDIA** (Ex: GTX 1660 Super) | `h264_nvenc` / `hevc_nvenc` | Codificação ultrarrapida no chip dedicado NVENC. |
| **AMD Radeon** | `h264_amf` / `hevc_amf` | Aceleração nativa para placas RX/APUs Ryzen. |
| **Intel (Arc ou Integrada HD/Iris)** | `h264_qsv` / `hevc_qsv` | QuickSync da Intel, excelente para notebooks sem GPU dedicada. |
| **CPU Pura (Fallback)** | `libx264` (com Multithreading) | Uso otimizado de múltiplos núcleos caso o PC não possua GPU suportada. |

---

## 🛠️ 3. Etapas do Projeto (Desenvolvimento em Estágios)

### 📍 Etapa 1: Concepção & Decisões Arquiteturais (Em Andamento 🏁)
* [x] **1.1. Definição da Stack Principal & Visual:** **C++ com Qt / QML** 🏆 (Escolhida)
  * **Por que essa escolha?** Combina a velocidade extrema de inicialização e baixo consumo do C++ com a engine visual mais conceituada do mercado (Qt/QML), permitindo animações suaves fluidas na GPU sem parecer um software clássico dos anos 2000.
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
* [x] **2.2. Módulo de Detecção de GPU:** Criado o [GPUDetector.cpp](file:///c:/Users/Admin/Desktop/ProjetosCode/projetos/Baixar/src/GPUDetector.cpp) em C++ nativo com sondagem universal de hardware (identificando com perfeição a **NVIDIA GeForce GTX 1660 SUPER** e selecionando o codec `h264_nvenc`).
* [x] **2.3. Mídia Sem Perda (*Stream Copy*):** Estruturado o motor de download para união instantânea de faixas sem sobrecarga de processador.

---

### 📍 Etapa 3: Interface Gráfica e Experiência do Usuário (UI/UX) 🏁 (Concluído!)
* [x] **3.1. Design da Janela Principal:** Criada interface em Qt 6 com tema moderno (Dark Tech), campos de URL, qualidade e o diferencial de Recorte de Faixa de Tempo em [MainWindow.cpp](file:///c:/Users/Admin/Desktop/ProjetosCode/projetos/Baixar/src/MainWindow.cpp).
* [x] **3.2. Gerenciador de Downloads:** Implementada tabela por tarefa, concorrência de 1 a 5 downloads, progresso em tempo real, cancelamento individual/global e resumo único da sessão.
* [x] **3.3. Indicador Visual de Hardware:** Exibido na linha de frente um cartão verde brilhante de "Placa Dedicada Ativa (NVIDIA NVENC / GTX 1660 SUPER)" destacando o processamento em hardware na tela!

---

### 📍 Etapa 4: Diferenciais Inteligentes & Polimento do MVP 🏁 (Concluído!)
* [x] **4.1. Recorte Inteligente de Faixa de Tempo:** Implementado corte direto nas streams sem precisar processar arquivos inteiros em disco.
* [x] **4.2. Polimento de UI/UX e Tratamento de Erros:** Seletor automático para a pasta de Downloads, botão de ação ciano para abrir o Explorer diretamente nos arquivos salvos e alertas limpos ao usuário.
* [x] **4.3. Testes Reais do MVP:** Validada união instantânea em 1 segundo via Stream Copy no Windows e suporte a aceleração NVIDIA NVENC. Histórico e Gerenciador de Pastas: Abrir rapidamente a pasta com os arquivos já baixados.

---

### 📍 Etapa 5: Empacotamento e Preparação para Distribuição (Deploy) 🏁 (Concluído!)
* [x] **5.1. Bundle de Dependências:** Todos os binários de aceleração (FFmpeg/NVENC) e bibliotecas nativas do Qt 6 foram embutidos junto ao executável principal C++17.
* [x] **5.2. Pacote Portável Autônomo:** Gerado o pacote ZIP auto-executável sem necessidade de instalação em `dist\NeoVDownloader_V1.0_Portable.zip`.
* [x] **5.3. Criar Instalador Oficial:** Gerado com *Inno Setup 6* o instalador moderno com compressão LZMA2, atalho de Área de Trabalho e suporte ao Português em `dist\NeoVDownloader_V1.0_Setup.exe`.

---
*Status do Projeto: 🏆 ROADMAP 100% CONCLUÍDO COM SUCESSO! O NeoVDownloader V1.0 está compilado em C++17 puro + Qt 6, empacotado e pronto para uso profissional e distribuição ao mundo!*
