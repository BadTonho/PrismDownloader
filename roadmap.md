# 🗺️ Roadmap - App de Download Rápido com Aceleração por GPU

Este documento descreve o planejamento e as etapas de desenvolvimento para a criação de um aplicativo desktop de download de vídeos (YouTube e outras plataformas), focado em **alta velocidade**, **baixo uso de CPU** e **suporte multiplataforma de placa de vídeo (GPU)** para futura distribuição.

---

## 🚀 1. Visão Geral do Projeto (Overview)

* **Objetivo:** Substituir ferramentas legadas e lentas (como o aTube Catcher) por um aplicativo desktop moderno, inteligente e ágil, capaz de lidar com vídeos muito longos (cursos, lives, podcasts) sem trarações ou lentidão no computador.
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
  * 🏎️ **Motor "Zero-Lag" para Vídeos Longos (Stream Copy):** Evitar o grande problema do aTube Catcher alavancando junção instantânea (*muxing*) sem sobrecarregar a CPU.
  * 🚀 **Aceleração por Hardware em GPU:** Acionar chips dedicados de vídeo nas conversões de arquivos (NVIDIA NVENC para a sua GTX 1660 Super, e detecção limpa para AMD/Intel).
  * 📚 **Suporte a Playlists e Vídeos Longos:** Gerenciador estável de fila, mostrando velocidade real (MB/s) e tempo restante.
  * ✂️ **Recorte Inteligente de Faixa de Tempo:** Poder recortar e baixar apenas um trecho específico de um vídeo ou live (Ex: das `10:15` às `18:40`) sem descarregar horas inteiras desnecessariamente.

---

### 📍 Etapa 2: Motor Core & Inteligência de Hardware em C++ 🏁 (Concluído!)
* [x] **2.1. Integração C++ <> Motores de Mídia:** Criada a classe [DownloadEngine.cpp](file:///c:/Users/Admin/Desktop/ProjetosCode/projetos/Baixar/src/DownloadEngine.cpp) em C++17 puro (via `std::thread` e pipes assíncronos) com callbacks para progresso em tempo real e suporte a recorte de faixa de tempo.
* [x] **2.2. Módulo de Detecção de GPU:** Criado o [GPUDetector.cpp](file:///c:/Users/Admin/Desktop/ProjetosCode/projetos/Baixar/src/GPUDetector.cpp) em C++ nativo com sondagem universal de hardware (identificando com perfeição a **NVIDIA GeForce GTX 1660 SUPER** e selecionando o codec `h264_nvenc`).
* [x] **2.3. Mídia Sem Perda (*Stream Copy*):** Estruturado o motor de download para união instantânea de faixas sem sobrecarga de processador.

---

### 📍 Etapa 3: Interface Gráfica e Experiência do Usuário (UI/UX) 🏁 (Concluído!)
* [x] **3.1. Design da Janela Principal:** Criada interface em Qt 6 com tema moderno (Dark Tech), campos de URL, qualidade e o diferencial de Recorte de Faixa de Tempo em [MainWindow.cpp](file:///c:/Users/Admin/Desktop/ProjetosCode/projetos/Baixar/src/MainWindow.cpp).
* [x] **3.2. Gerenciador de Downloads:** Implementada barra de progresso em tempo real (`QProgressBar`), velocidade em MB/s e botão de cancelamento integrados de forma thread-safe com o nosso worker C++.
* [x] **3.3. Indicador Visual de Hardware:** Exibido na linha de frente um cartão verde brilhante de "Placa Dedicada Ativa (NVIDIA NVENC / GTX 1660 SUPER)" destacando o processamento em hardware na tela!

---

### 📍 Etapa 4: Refinamentos e Funcionalidades Avançadas
* [ ] **4.1. Suporte para Vídeos Muito Longos & Conectividade Resiliente:** Reconectar automaticamente em caso de falha temporária no Wi-Fi/Internet.
* [ ] **4.2. Recorte de Trecho (Opcional):** Opção de baixar apenas do minuto *X* até o minuto *Y* sem precisar baixar um vídeo de 5 horas inteiro!
* [ ] **4.3. Histórico e Gerenciador de Pastas:** Abrir rapidamente a pasta com os arquivos já baixados.

---

### 📍 Etapa 5: Empacotamento e Preparação para Distribuição (Deploy)
* [ ] **5.1. Bundle de Dependências:** Configurar para embutir automaticamente os binários do `FFmpeg` para o Windows.
* [ ] **5.2. Compilação para `.exe` autônomo:** Utilizar ferramentas como *Nuitka* ou *PyInstaller* para gerar um executável de performance máxima.
* [ ] **5.3. Criar Instalador Amigo:** (Opcional) Usar *Inno Setup* para criar um instalador oficial ("Setup.exe") com ícone personalizado e atalho na Área de Trabalho.

---
*Status do Projeto: 🏆 Etapa 3 Concluída! Janela Gráfica (UI/UX) com tema Dark Tech compilada com Qt 6.7 e executando limpa na tela do usuário com indicador NVIDIA NVENC ativo.*
