# 🧩 Referência de Módulos e Código Fonte — Prism Downloader

<p align="center">
  <a href="../MODULES.md">🇺🇸 Read the English version here.</a>
</p>

Este documento contém a especificação técnica detalhada de todas as classes, estruturas de dados, enums e serviços do **Prism Downloader**.

---

## 📑 Sumário dos Módulos

1. [Estruturas Básicas e Modelos de Dados](#1-estruturas-básicas-e-modelos-de-dados)
   - `MediaItem.h`
   - `DownloadProfile.h`
2. [Gerenciador de Downloads (`DownloadManager`)](#2-gerenciador-de-downloads-downloadmanager)
3. [Gerenciador de Conversões (`ConversionManager`)](#3-gerenciador-de-conversões-conversionmanager)
4. [Detector de Hardware & GPU (`GPUDetector`)](#4-detector-de-hardware--gpu-gpudetector)
5. [Resolvedor de Ferramentas de Mídia (`MediaToolResolver`)](#5-resolvedor-de-ferramentas-de-mídia-mediatoolresolver)
6. [Serviço de Atualização do App (`AppUpdateService`)](#6-serviço-de-atualização-do-app-appupdateservice)
7. [Serviço de Atualização do yt-dlp (`YtDlpUpdateService`)](#7-serviço-de-atualização-do-yt-dlp-ytdlpupdateservice)
8. [Auxiliar de Atualização Portátil (`PortableUpdateHelper` & `PortableUpdateCommon`)](#8-auxiliar-de-atualização-portátil-portableupdatehelper--portableupdatecommon)
9. [Interface Principal (`MainWindow`)](#9-interface-principal-mainwindow)

---

## 1. Estruturas Básicas e Modelos de Dados

### 1.1. `MediaItem.h`
Define os estados fundamentais de uma mídia no ciclo de vida de processamento:

```cpp
enum class DownloadStatus {
    Queued,         // Na fila aguardando slot de concorrência disponível
    Downloading,    // yt-dlp em execução baixando streams de vídeo/áudio
    Muxing,         // Junção de faixas (Stream Copy via FFmpeg)
    ConvertingGPU,  // Transcodificação ativa via GPU ou CPU
    Cancelling,     // Sinal de cancelamento emitido, aguardando término de processo
    Completed,      // Download e conversões concluídos com sucesso
    Error,          // Falha de rede, sintaxe de URL ou erro no processo externo
    Cancelled       // Interrompido com sucesso pelo usuário
};
```

#### Struct `MediaItem`
* `std::string url`: URL original fornecida pelo usuário.
* `std::string title`: Título da mídia extraído da plataforma.
* `std::string quality`: Perfil de qualidade selecionado (ex: `"1080p"`, `"4K"`, `"MP3"`).
* `std::string speed`: Velocidade instantânea formatada (ex: `"12.5 MB/s"`).
* `std::string eta`: Tempo restante estimado (ex: `"01:30"`).
* `double progress`: Porcentagem numérica de 0.0 a 100.0.
* `DownloadStatus status`: Estado atual da tarefa.
* `bool isAudioOnly() const`: Método utilitário que avalia se a qualidade requer apenas áudio (procura por `"MP3"`, `"FLAC"`, `"AUDIO"`).

---

### 1.2. `DownloadProfile.h`
Mapeia opções de resolução selecionadas na interface gráfica para seletores de formato avançados do `yt-dlp`:

* `formatSelectorForQuality(const std::string &quality)`:
  - `"1080P"` $\rightarrow$ `bv*[height<=1080]+ba/b[height<=1080]`
  - `"720P"` $\rightarrow$ `bv*[height<=720]+ba/b[height<=720]`
  - `"4K"` $\rightarrow$ `bv*[height<=2160]+ba/b[height<=2160]`
  - Padrão $\rightarrow$ `bv*+ba/b` (Melhor vídeo e melhor áudio disponíveis).

---

## 2. Gerenciador de Downloads (`DownloadManager`)

**Arquivos:** `src/DownloadManager.h` e `src/DownloadManager.cpp`

Classe central que orquestra a execução assíncrona do `yt-dlp` com suporte a limites de paralelismo.

### Tipos e Estruturas
* `using DownloadId = quint64`: Identificador único auto-incremental para cada download.
* `struct DownloadRequest`:
  - `QUrl url`: Endereço da mídia ou vídeo individual.
  - `QString quality`: Seletor de qualidade.
  - `QString timeRange`: Faixa de tempo para recorte (*time-slice*, ex: `"00:01:00-00:03:00"`).
  - `QString outputDirectory`: Caminho absoluto do diretório de destino.
* `struct EnqueueResult`:
  - `bool accepted`: Indica se a requisição foi aceita.
  - `DownloadId id`: ID atribuído ao download.
  - `QString error`: Mensagem em caso de rejeição (ex: URL inválida ou encerramento do app).

### Métodos Públicos
* `EnqueueResult enqueueDownload(const DownloadRequest &request)`: Valida os parâmetros e adiciona a tarefa à fila.
* `bool cancelDownload(DownloadId id)`: Interrompe graciosamente o processo associado ao ID fornecido.
* `void cancelAll()`: Interrompe todos os downloads ativos e limpa a fila de espera.
* `void setConcurrencyLimit(int limit)`: Ajusta o limite de downloads simultâneos (de 1 a 5).
* `int concurrencyLimit() const`: Retorna o limite configurado.
* `int activeCount() const`: Quantidade de tarefas executando no momento.
* `int pendingCount() const`: Quantidade de tarefas aguardando na fila.
* `bool hasWork() const`: Retorna `true` se houver downloads ativos ou pendentes.

### Sinais Qt (`signals`)
* `void jobProgress(DownloadId id, double percent, const QString &speed, const QString &eta)`: Atualização periódica de telemetria.
* `void jobStatus(DownloadId id, DownloadStatus status, const QString &message)`: Mudança de estado da tarefa.
* `void jobCompleted(DownloadId id, const QString &filePath)`: Emitido quando o arquivo final foi salvo em disco.
* `void jobLog(DownloadId id, const QString &message)`: Linhas de log brutas capturadas do subprocesso.
* `void queueStateChanged(int active, int pending)`: Notificação de alteração de carga da fila.
* `void queueIdle()`: Emitido quando todas as tarefas da fila foram concluídas.

---

## 3. Gerenciador de Conversões (`ConversionManager`)

**Arquivos:** `src/ConversionManager.h` e `src/ConversionManager.cpp`

Controla a fila sequencial FIFO de transcodificação de arquivos com `FFmpeg`, evitando saturação da GPU ou CPU.

### Tipos e Estruturas
* `using ConversionId = quint64`: Identificador único da conversão.
* `struct ConversionRequest`:
  - `DownloadId ownerDownloadId`: ID do download original (se originado da automação pós-download) ou `0` para conversões manuais.
  - `QString inputFile`: Arquivo de origem.
  - `QString format`: Formato desejado (`"MP3"`, `"FLAC"`, `"MP4"`, `"MKV"`, etc.).
  - `QString outputDirectory`: Diretório de saída.
  - `GPUType gpuType`: Tipo de aceleração selecionado (`NVIDIA`, `AMD`, `INTEL`, `VAAPI`, `CPU_ONLY`).
  - `QString gpuCodec`: Codec específico (ex: `"h264_nvenc"`, `"h264_vaapi"`).
  - `QString gpuDevice`: Caminho do dispositivo de hardware (ex: `"/dev/dri/renderD128"` no Linux).

### Métodos Públicos
* `ConversionEnqueueResult enqueueConversion(const ConversionRequest &request)`: Insere uma nova conversão na fila FIFO.
* `bool cancelConversion(ConversionId id)`: Cancela uma conversão específica.
* `void cancelByDownloadId(DownloadId downloadId)`: Cancela a conversão associada a um determinado download.
* `void cancelAllAutomatic()`: Limpa conversões agendadas automaticamente pelo fluxo de download.
* `bool hasWork() const`: Indica se há conversões ativas ou pendentes.

### Sinais Qt (`signals`)
* `void conversionQueued(ConversionId id, DownloadId ownerDownloadId, int position)`: Posição na fila.
* `void conversionStatus(ConversionId id, DownloadId ownerDownloadId, const QString &message)`: Status textual.
* `void conversionCompleted(ConversionId id, DownloadId ownerDownloadId, const QString &outputFile)`: Conversão finalizada.
* `void conversionFailed(ConversionId id, DownloadId ownerDownloadId, const QString &message)`: Falha com relatório de erro.
* `void conversionCancelled(ConversionId id, DownloadId ownerDownloadId)`: Conversão abortada.
* `void conversionLog(ConversionId id, DownloadId ownerDownloadId, const QString &message)`: Saída de log do FFmpeg.

---

## 4. Detector de Hardware & GPU (`GPUDetector`)

**Arquivos:** `src/GPUDetector.h` e `src/GPUDetector.cpp`

Módulo em C++ puro responsável pela sondagem de placas de vídeo e verificação de suporte a encoders de aceleração por hardware.

### Enums e Métodos
* `enum class GPUType { NVIDIA, AMD, INTEL, VAAPI, CPU_ONLY };`
* `void detect(bool verbose = false)`: Executa a sondagem no sistema. Se `verbose = true`, imprime relatório detalhado no console (usado pelo argumento `--diagnose-gpu`).
* `GPUType getGPUType() const`: Retorna a fabricante/tecnologia identificada.
* `std::string getGPUName() const`: Nome comercial do adaptador (ex: `"NVIDIA GeForce GTX 1660 SUPER"`).
* `std::string getRecommendedCodec() const`: Nome do encoder do FFmpeg (ex: `"h264_nvenc"`, `"h264_vaapi"`, `"libx264"`).
* `std::string getHardwareDevice() const`: Identificador de dispositivo (ex: `"/dev/dri/renderD128"`).
* `std::string getDiagnostic() const`: Texto explicativo com o diagnóstico completo da detecção.
* `bool hasHardwareAcceleration() const`: Retorna `true` se uma GPU compatível foi validada com sucesso.

---

## 5. Resolvedor de Ferramentas de Mídia (`MediaToolResolver`)

**Arquivos:** `src/MediaToolResolver.h` e `src/MediaToolResolver.cpp`

Localiza e prioriza os executáveis do `yt-dlp` e `FFmpeg` no sistema operacional.

### Enums e Estruturas
* `enum class MediaTool { YtDlp, Ffmpeg };`
* `enum class MediaToolSource { Unavailable, Explicit, UserUpdate, Path, Bundled };`
* `struct MediaToolInfo`:
  - `QString path`: Caminho absoluto do binário.
  - `QString version`: Versão extraída do binário (ex: `"2026.03.01"`).
  - `MediaToolSource source`: Origem da descoberta.
  - `bool isAvailable() const`: Retorna `true` se o executável foi localizado.

### Métodos Estáticos Principais
* `resolve(MediaTool tool, const QString &programPath = {})`: Retorna o caminho do binário mais adequado.
* `resolveInfo(MediaTool tool, const QString &programPath = {})`: Retorna o `MediaToolInfo` completo.
* `selectYtDlpCandidate(const QList<MediaToolInfo> &candidates)`: Compara versões entre diretório de usuário, bundle e PATH, escolhendo a mais recente.
* `isVersionNewer(const QString &candidate, const QString &current)`: Compara semanticamente duas versões de software.
* `ytDlpUserPath()`: Retorna o caminho onde atualizações locais do yt-dlp são salvas (ex: `%LOCALAPPDATA%/PrismDownloader/yt-dlp.exe` ou `~/.local/share/prism-downloader/yt-dlp`).

---

## 6. Serviço de Atualização do App (`AppUpdateService`)

**Arquivos:** `src/AppUpdateService.h` e `src/AppUpdateService.cpp`

Implementa a verificação, download e validação criptográfica de atualizações do Prism Downloader via GitHub Releases.

### Características e Métodos
* `enum class AppUpdatePackageKind { WindowsSetup, WindowsPortable, LinuxDeb };`
* `struct AppUpdateReleaseInfo`: Versão da release, nome do arquivo de pacote esperado, hash SHA-256 e URL de download.
* `static AppUpdateReleaseInfo parseRelease(...)`: Analisa a resposta da API do GitHub, valida o manifesto e seleciona o pacote da plataforma atual.
* `void checkLatestRelease()`: Inicia verificação assíncrona na nuvem via `QNetworkAccessManager`.
* `void downloadLatestRelease()`: Realiza o download do instalador/pacote com cálculo contínuo de hash SHA-256 em streaming (`QCryptographicHash`).

---

## 7. Serviço de Atualização do yt-dlp (`YtDlpUpdateService`)

**Arquivos:** `src/YtDlpUpdateService.h` e `src/YtDlpUpdateService.cpp`

Responsável por manter o motor `yt-dlp` na versão Nightly mais recente sem a necessidade de atualizar todo o aplicativo.

* `checkLatestRelease()`: Consulta o canal oficial de releases Nightly do `yt-dlp` no GitHub.
* `installLatestRelease()`: Baixa os arquivos de checksum `SHA2-256SUMS`, valida a integridade do binário correspondente à plataforma (`yt-dlp.exe` ou `yt-dlp_linux`) e grava atomicamente no diretório de dados do usuário (`ytDlpUserPath()`).

---

## 8. Auxiliar de Atualização Portátil (`PortableUpdateHelper` & `PortableUpdateCommon`)

**Arquivos:** `src/PortableUpdateCommon.h`, `src/PortableUpdateCommon.cpp` e `src/PortableUpdateHelper.cpp`

Binário auxiliar desacoplado (`portable-update-helper.exe`) utilizado exclusivamente para atualizar a versão portátil em tempo de execução no Windows.

### Fluxo de Operação do Helper
1. Recebe via linha de comando: `--parent-pid <PID>`, `--archive <ZIP>`, `--target <PASTA>`.
2. Aguarda o processo pai (Prism Downloader principal) encerrar completamente via Win32 `WaitForSingleObject(handle, 120000)`.
3. Extrai o pacote ZIP para uma pasta de preparação temporária (`.prism-update-staging-XXXXXX`).
4. Realiza o backup atômico da pasta antiga (`.prism-update-backup-<PID>`).
5. Renomeia a pasta de staging para a pasta definitiva de instalação. Em caso de falha em qualquer etapa, executa rollback imediato do backup.
6. Inicia o novo `PrismDownloader.exe` desacoplado (`QProcess::startDetached`) e limpa arquivos temporários.

---

## 9. Interface Principal (`MainWindow`)

**Arquivos:** `src/MainWindow.h` e `src/MainWindow.cpp`

Janela principal em Qt 6 que integra todos os serviços, gerencia a interface gráfica (Dark Tech), monitora tabelas e despacha ações do usuário.

### Seções da Janela
* **Sidebar:** Navegação entre abas (`Downloads`, `Biblioteca`, `Conversor`, `Logs`, `Info & Atualizações`).
* **Monitor de Fila:** Tabela interativa de downloads exibindo Título, Qualidade, Velocidade, Tempo Restante, Barra de Progresso e Ações individuais.
* **Terminal de Logs:** Visualizador de saída com filtros sob demanda:
  - `0`: Todos os logs.
  - `1`: Apenas saídas de processos externos (`yt-dlp` e `ffmpeg`).
  - `2`: Isolamento visual de erros e alertas.
  - `3`: Mensagens gerais de sistema e ciclo de vida.
* **Seletor de Playlists:** Janela de pré-visualização para carregar listas de vídeos do YouTube, permitindo ao usuário marcar quais faixas deseja baixar antes de enfileirar.
