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

### 📍 Etapa 2: Motor Core & Inteligência de Hardware em C++ (Em Andamento 🏁)
* [ ] **2.1. Integração C++ <> Motores de Mídia:** Criar classe gerenciadora em C++ (via `QProcess` ou pipes) para invocar os binários otimizados de `yt-dlp` e `ffmpeg` em background, capturando velocidade, progresso e miniatura sem congelar a interface.
* [ ] **2.2. Módulo de Detecção de GPU:** Lógica que verifica placas NVIDIA (ex: GTX 1660 Super), AMD ou Intel na máquina do usuário para selecionar o codec mais rápido.
* [ ] **2.3. Mídia Sem Perda (*Stream Copy*):** Estratégia para apenas juntar streams originais e evitar qualquer processamento de CPU sempre que possível.

---

### 📍 Etapa 3: Interface Gráfica e Experiência do Usuário (UI/UX)
* [ ] **3.1. Design da Janela Principal:**
  * Campo para colar a URL com botão "Colar & Analisar".
  * Pré-visualização com Título do Vídeo, Duração e Capa (Thumbnail).
  * Seletor de Formato (Vídeo MP4 / Áudio MP3) e Qualidade.
* [ ] **3.2. Gerenciador de Downloads:**
  * Barra de Progresso animada em tempo real com indicador de velocidade (MB/s).
  * Botão de cancelar ou pausar/retomar downloads.
* [ ] **3.3. Indicador Visual de Hardware:** Um pequeno ícone ou texto mostrando ao usuário: *"⚡ Acelerado via NVIDIA NVENC"* ou *"⚡ Cópia Direta Sem Perda"*, para impressionar e transmitir velocidade!

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
*Status do Projeto: 🟡 Etapa 2 Em Andamento: Construindo a arquitetura em C++ e Qt para Detecção de GPU e Motor de Download (QProcess).*
