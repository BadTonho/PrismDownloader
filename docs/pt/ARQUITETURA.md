# 🏗️ Arquitetura do Sistema — Prism Downloader

<p align="center">
  <a href="../ARCHITECTURE.md">🇺🇸 Read the English version here.</a>
</p>

Este documento detalha a arquitetura de software, fluxo de dados e o modelo de concorrência do **Prism Downloader**, projetado em **C++17** com **Qt 6** para máxima velocidade de execução, responsividade de interface e estabilidade operacional.

---

## 📐 1. Visão Geral da Arquitetura

O Prism Downloader adota um padrão arquitetural desacoplado, baseado no modelo reativo de **Sinais e Slots do Qt** e no **isolamento de processos externos (`QProcess`)**. Essa separação garante que o processamento pesado de rede (download de fluxos com `yt-dlp`) e de multimídia (muxing e transcodificação com `FFmpeg`) ocorra em processos filhos assíncronos, sem bloquear a thread principal da interface gráfica (UI Thread).

```mermaid
flowchart TD
    subgraph UI_Layer ["Camada de Apresentação (GUI - Qt 6)"]
        MW["MainWindow (Interface Dark Tech)"]
        DL_Tab["Aba de Downloads"]
        LIB_Tab["Biblioteca de Mídias"]
        CONV_Tab["Conversor de Vídeo"]
        LOG_Tab["Terminal de Logs & Telemetria"]
        UPD_Tab["Central de Atualizações"]
    end

    subgraph Core_Services ["Camada de Serviços & Lógica Core (C++17)"]
        DM["DownloadManager\n(Fila Concorrente 1-5)"]
        CM["ConversionManager\n(Fila FIFO Estrita 1-Job)"]
        GD["GPUDetector\n(Sondagem Universal de Hardware)"]
        MTR["MediaToolResolver\n(Resolução Dinâmica de Binários)"]
        AUS["AppUpdateService\n(Validação SHA-256)"]
        YUS["YtDlpUpdateService\n(Updater Autônomo Nightly)"]
    end

    subgraph Engine_Layer ["Camada de Execução Externa (Subprocessos)"]
        YTDLP["yt-dlp (Nightly Engine)"]
        FFMPEG["FFmpeg (Stream Copy / NVENC / VAAPI)"]
    end

    MW --> DL_Tab & LIB_Tab & CONV_Tab & LOG_Tab & UPD_Tab
    DL_Tab --> DM
    CONV_Tab --> CM
    DM --> CM
    DM --> YTDLP
    CM --> FFMPEG
    DM -.-> MTR
    CM -.-> MTR
    UPD_Tab --> AUS & YUS
    MW -.-> GD
```

---

## 🔄 2. Fluxo de Dados Ponta a Ponta

O ciclo de vida de uma operação de download e processamento segue um fluxo estruturado com estados determinísticos:

```mermaid
sequenceDiagram
    autonumber
    actor User as Usuário
    participant UI as MainWindow
    participant DM as DownloadManager
    participant YTDLP as Processo yt-dlp
    participant CM as ConversionManager
    participant FFMPEG as Processo FFmpeg
    participant FS as Sistema de Arquivos

    User->>UI: Insere URL, seleciona qualidade e clica em Iniciar
    UI->>DM: enqueueDownload(DownloadRequest)
    DM->>DM: Enfileira job e verifica limite de concorrência
    DM->>YTDLP: Inicia QProcess com argumentos otimizados
    loop Leitura de Saída
        YTDLP-->>DM: Linhas de stdout/stderr (progresso, velocidade, ETA)
        DM-->>UI: jobProgress(...) / jobLog(...)
        UI-->>User: Atualiza barra de progresso e tabela ao vivo
    end
    YTDLP->>DM: Processo finalizado (ExitCode 0)
    DM->>UI: jobCompleted(DownloadId, filePath)
    
    alt Requer Conversão / Extração de Áudio
        UI->>CM: enqueueConversion(ConversionRequest)
        CM->>CM: Enfileira na fila FIFO
        CM->>FFMPEG: Inicia QProcess com codec acelerado (ou CPU)
        FFMPEG-->>CM: Linhas de log / progresso
        FFMPEG->>CM: Finalizado com sucesso
        CM->>UI: conversionCompleted(...)
    end

    UI->>FS: Atualiza lista da Biblioteca de Mídias
    UI-->>User: Notifica conclusão e disponibiliza reprodução/abertura de pasta
```

---

## ⚙️ 3. Modelo de Concorrência e Gestão de Filas

### 3.1. Fila de Downloads (`DownloadManager`)
* **Concorrência Configurável:** O usuário pode definir dinamicamente entre **1 e 5 downloads simultâneos** através da UI (`m_concurrencySpin`).
* **Agendador Reativo (`schedule()`):** Sempre que uma tarefa é finalizada, cancelada ou quando o limite de concorrência é aumentado, o agendador ativa a próxima tarefa pendente da fila (`m_pending`).
* **Parser de Telemetria:** A saída padrão do `yt-dlp` é lida linha a linha através de buffers de texto (`readProcessOutput()`), extraindo com expressões regulares:
  - Percentual concluído (`%`).
  - Velocidade de transferência (ex: `14.2MiB/s`).
  - Tempo estimado de conclusão (ETA).
  - Destino final do arquivo (`[Merger] Merging formats into "..."` ou `[download] Destination: ...`).

### 3.2. Fila de Conversão FIFO (`ConversionManager`)
* **Execução Sequencial Estrita (Single-Job):** Ao contrário dos downloads de rede, transcodificações de áudio e vídeo competem intensamente por recursos de hardware (memória VRAM, canais de encoder NVENC/AMF/QSV ou threads de CPU).
* Para evitar travamento do driver de vídeo ou saturação de 100% dos núcleos do processador, o `ConversionManager` processa rigorosamente **uma conversão por vez** (`m_active`), mantendo as demais em espera ordenada (`m_pending`).

---

## 🛡️ 4. Gerenciamento de Processos e Isolamento de Sessão

### 4.1. Prevenção de Processos Zumbis
* O Prism Downloader utiliza `QProcess::start(program, arguments)` com passagem direta de argumentos estruturados (evitando injeção de shell e necessidade de abrir janelas `cmd.exe` ou `sh`).
* No **Windows**, subprocessos são vinculados e monitorados pelo identificador de processo (`Q_PID`).
* No **Linux**, cada download cria uma nova sessão de processo (`setsid`), garantindo que se o `yt-dlp` disparar instâncias filhas de `ffmpeg` para pós-processamento, o encerramento ou cancelamento da tarefa elimine toda a árvore de processos sem deixar órfãos em background.

### 4.2. Encerramento Seguro da Aplicação (`closeEvent`)
* Ao fechar a janela principal (`MainWindow::closeEvent`), o aplicativo realiza uma finalização graciosa:
  1. Cancela todos os downloads pendentes e em execução (`m_downloadManager->cancelAll()`).
  2. Cancela a fila de conversões automáticas (`m_conversionManager->cancelAllAutomatic()`).
  3. Desconecta sinais de rede do sistema de atualização.
  4. Aguarda a liberação dos descritores de arquivo antes de sair do loop de eventos.

---

## 🧩 5. Resolução Dinâmica de Ferramentas (`MediaToolResolver`)

O Prism Downloader implementa uma estratégia multicamadas para encontrar os binários do `yt-dlp` e `FFmpeg`:

```mermaid
graph TD
    Start["Início da Resolução"] --> CheckUser["1. Existe binário atualizado pelo usuário em AppData / .local/share?"]
    CheckUser -- Sim --> CheckVer1["Compara versão com demais fontes"]
    CheckUser -- Não --> CheckBundled["2. Existe binário embutido na pasta do app?"]
    CheckBundled -- Sim --> CheckVer2["Compara versão com PATH"]
    CheckBundled -- Não --> CheckPath["3. Existe no PATH do Sistema?"]
    CheckVer1 --> PickNewest["Seleciona a versão mais recente"]
    CheckVer2 --> PickNewest
    CheckPath --> UsePath["Utiliza do PATH"]
    PickNewest --> Ready["Binário Selecionado e Pronto para Execução"]
```

Essa lógica assegura que:
1. O usuário final que nunca configurou variáveis de ambiente possa usar o app imediatamente (graças aos binários bundled).
2. Atualizações do `yt-dlp` possam ser instaladas pelo próprio aplicativo em tempo real.
3. Se o usuário tiver uma versão global mais recente instalada no sistema operacional, ela seja aproveitada automaticamente.
