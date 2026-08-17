# 🎨 Guia de Interface do Usuário (UI/UX) — Prism Downloader

Este documento apresenta o guia visual e funcional da interface do **Prism Downloader**, desenvolvida com **Qt 6** no moderno tema **Dark Tech**, priorizando usabilidade, alta legibilidade e acesso rápido a recursos avançados.

---

## 🖥️ 1. Filosofia de Design e Layout Geral

A interface do Prism Downloader foi estruturada em torno de uma **Sidebar de Navegação Lateral Fixa** e uma **Área de Conteúdo Dinâmica (`QStackedWidget`)**, permitindo alternar instantaneamente entre as ferramentas sem perder o estado das operações em andamento.

```
+-------------------------------------------------------------------------------+
|  💎 PRISM DOWNLOADER                              [—]  [□]  [✕] (Janela Qt 6) |
+----------------+--------------------------------------------------------------+
|  [📥 Downloads] |  URL: [ https://www.youtube.com/watch?v=...         ] [▶ INICIAR] |
|  [📚 Biblioteca]|  Qualidade: [ 1080p Full HD ▼ ]   Corte: [ 00:01:00 - 00:03:00 ] |
|  [🔄 Conversor] |  Destino:   [ C:/Users/.../Downloads           ] [📁 Procurar]   |
|  [📡 Logs / CLI]|  ---------------------------------------------------------- |
|  [⚙️ Updates]   |  TABELA DE DOWNLOADS EM TEMPO REAL (Concorrência: [ 2 ▲▼ ])   |
|                |  • Item 1 | 1080p | [██████████░░░░] 68% | 14.2 MB/s | ETA 00:15   |
|  [📁 Abrir Pasta|  • Item 2 | MP3   | [██████████████] 100%| Concluído | 00:00       |
|  [🔔 Nova Versão|                                                              |
+----------------+--------------------------------------------------------------+
```

---

## 📑 2. Detalhamento de Cada Aba

### 2.1. 📥 Aba de Downloads (Painel Principal)
A tela de downloads concentra todas as operações de captura de fluxos da web:

1. **Campo de URL:** Aceita links de vídeos únicos, lives, clipes e playlists completas do YouTube e centenas de outras plataformas suportadas pelo `yt-dlp`.
2. **Seletor de Qualidade / Perfil:**
   - `4K Ultra HD` (resolução máxima até 2160p a 60fps).
   - `1080p Full HD` (padrão de alta definição e compatibilidade).
   - `720p HD` (opção leve para conexões mais lentas).
   - `Áudio MP3 / FLAC` (extração automática de trilha sonora com metadados).
3. **Recorte Inteligente de Faixa de Tempo (*Time-Slice Trimming*):**
   - Campos de início e término no formato `HH:MM:SS` ou `MM:SS` (ex: `00:02:15` a `00:05:40`).
   - O Prism solicita apenas a faixa selecionada ao servidor, economizando banda e tempo de download.
4. **Controle de Concorrência:**
   - Permite ajustar de **1 a 5 downloads simultâneos** sem reiniciar o aplicativo.
5. **Tabela de Monitoramento ao Vivo:**
   - Exibe título, qualidade, barra de progresso individual, velocidade instantânea em MB/s, tempo estimado de conclusão (ETA) e status da tarefa.
   - Suporte a cancelamento individual ou global (`Cancelar Todos`).

---

### 2.2. 📚 Aba de Biblioteca de Mídias
Organizador local integrado para os arquivos salvos:

* **Varredura Automática:** Lista todos os vídeos e áudios presentes na pasta de downloads configurada.
* **Informações Detalhadas:** Nome do arquivo, extensão, tamanho em disco e data de modificação.
* **Ações Rápidas:**
  - Duplo clique sobre qualquer item reproduz a mídia diretamente no tocador padrão do sistema.
  - Botão dedicado para abrir o Explorador de Arquivos (Windows Explorer / Nautilus / Dolphin) com o arquivo selecionado.

---

### 2.3. 🔄 Aba de Conversor de Mídia
Permite converter e comprimir mídias locais do computador sem precisar baixá-las novamente:

* **Seleção de Origem:** Suporte a arrastar e soltar ou navegar por qualquer formato de vídeo/áudio comum (`.mp4`, `.mkv`, `.webm`, `.avi`, `.mov`, `.flv`, `.ts`, `.mp3`, `.wav`, `.aac`, `.flac`, `.ogg`).
* **Formatos de Saída:** Conversão para formatos modernos de vídeo ou extração para áudio de alta fidelidade.
* **Indicador Visual de Hardware:** Exibe um badge destacando a placa de vídeo ativa e o encoder em uso (ex: `NVIDIA NVENC (h264_nvenc)` ou `VAAPI (h264_vaapi)`).
* **Fila Segura FIFO:** Assegura que apenas uma conversão pesada seja processada por vez, preservando a estabilidade térmica e operacional do computador.

---

### 2.4. 📡 Aba de Terminal de Logs & Telemetria
Visualizador de console em tempo real para monitorar o comportamento interno dos processos:

```
[FILTROS DISPONÍVEIS NA BARRA SUPERIOR]
[🌐 Todos os Logs]  [⚙️ Processos]  [❌ Erros]  [📌 Sistema / Geral]  [🧹 Limpar Terminal]
```

* 🌐 **Todos os Logs:** Stream completo de eventos.
* ⚙️ **Processos:** Saída bruta detalhada dos comandos do `yt-dlp` e `FFmpeg`.
* ❌ **Erros:** Destaque visual imediato em vermelho para mensagens de erro, problemas de conexão e alertas.
* 📌 **Sistema / Geral:** Logs de ciclo de vida do aplicativo, detecção de hardware e checagem de atualizações.
* 🧹 **Limpar Terminal:** Esvazia o buffer de exibição da tela.

---

### 2.5. ⚙️ Aba de Configurações & Central de Atualizações
Painel de diagnóstico de hardware e gerenciamento de versões:

1. **Card de Diagnóstico de GPU:**
   - Modelo da placa gráfica identificada pelo sistema.
   - Codecs acelerados suportados.
   - Status de aceleração por hardware.
2. **Atualização do Aplicativo (Prism Core):**
   - Exibe a versão local atual e consulta releases oficiais no GitHub.
   - Botão **"Verificar Atualizações do Prism"**.
   - Barra de progresso com cálculo de hash SHA-256 em tempo real.
   - Opções para checar automaticamente na inicialização e habilitar download automático.
3. **Atualização do Motor `yt-dlp`:**
   - Exibe a versão do motor embutido e a origem (Bundle, Usuário ou PATH).
   - Botão **"Atualizar Motor yt-dlp"** para buscar a versão Nightly mais recente.

---

## 📋 3. Gerenciamento de Playlists do YouTube

Ao colar um link de playlist no campo de URL, o Prism Downloader aciona a janela de pré-visualização de playlist:

1. **Carregamento Assíncrono:** O Prism consulta a lista de vídeos sem travar a interface.
2. **Seleção Individual:** O usuário pode marcar ou desmarcar vídeos individualmente (ou utilizar o botão "Marcar Todos").
3. **Importação em Lote:** Ao confirmar, todos os itens selecionados são adicionados à fila de downloads de forma ordenada.
