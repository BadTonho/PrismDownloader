# 📚 Central de Documentação — Prism Downloader

Bem-vindo à documentação oficial do **Prism Downloader**, a suíte desktop desenvolvida em **C++17** e **Qt 6** focada em download de mídias em alta velocidade, extração de áudio, corte de trechos (*trimming*), conversão acelerada por GPU e atualizações automáticas com verificação criptográfica.

---

## 🧭 Índice da Documentação

A documentação está organizada de forma modular para facilitar a consulta por arquitetos de software, desenvolvedores e mantenedores do projeto:

| Documento | Descrição |
| :--- | :--- |
| 🏗️ [**Arquitetura do Sistema**](ARQUITETURA.md) | Visão geral da arquitetura em C++17/Qt 6, fluxo de dados ponta a ponta, modelo de concorrência com threads/processos desacoplados e gerenciamento de filas. |
| 🧩 [**Referência de Módulos & Código**](MODULOS.md) | Descrição técnica minuciosa de cada classe, estrutura de dados, enums, métodos públicos e sinais/slots do projeto. |
| ⚡ [**Aceleração por Hardware & GPU**](HARDWARE_GPU.md) | Detecção de GPU multiplataforma (Windows DXGI e Linux DRM/VAAPI), seleção de encoders (NVENC, AMF, QSV, VAAPI), fallback para CPU e CLI `--diagnose-gpu`. |
| 🛡️ [**Sistema de Atualização Segura**](AUTO_UPDATE.md) | Mecanismo de auto-atualização do Prism e do motor `yt-dlp`, validação de assinaturas digitais Ed25519, verificação de integridade SHA-256 e rotina de staging para versão portátil. |
| 🎨 [**Guia de Interface (UI/UX)**](INTERFACE_UI.md) | Detalhes da interface gráfica em Dark Tech, fluxo de abas (Downloads, Biblioteca, Conversão, Logs, Atualizações), filtros de telemetria em tempo real e seletor de playlists. |
| 📦 [**Compilação & Empacotamento**](BUILD_PACKAGING.md) | Guia passo a passo para configurar o ambiente e compilar no Windows (MSVC/Inno Setup) e Linux (GCC/Clang/CPack DEB), além da execução da suíte de testes CTest. |

---

## 🛠️ Stack Tecnológica & Dependências

* **Linguagem:** C++17 (padrão ISO C++17 estrito).
* **Framework Gráfico & Core:** Qt 6.7+ (`QtWidgets`, `QtNetwork`, `QtCore`, `QtGui`).
* **Criptografia & Assinaturas:** OpenSSL 1.1.1 / 3.x (`libcrypto` para verificação Ed25519).
* **Motores de Mídia Integrados:**
  * `yt-dlp` (Canal Nightly oficial para extração de stream e metadados).
  * `FFmpeg` (Muxing *Stream Copy*, transcodificação de vídeo e extração de áudio).
* **Build System:** CMake 3.16+ e CTest para suíte de testes unitários.
* **Empacotamento:**
  * Windows: Inno Setup 6 (Instalador `.exe`) e empacotamento ZIP (Portátil).
  * Linux: CPack / `dpkg-dev` (Pacote nativo `.deb` para Ubuntu 24.04 / Mint 22).

---

## 📌 Requisitos de Sistema

### Windows
* **Sistema Operacional:** Windows 10 (64-bit) ou Windows 11.
* **GPU Recomendada:** NVIDIA (GTX série 900+ / RTX), AMD Radeon (GCN/RDNA) ou Intel Core (6ª geração em diante / Intel Arc).
* **CPU Fallback:** Suporte universal em qualquer processador x86_64 multi-core.

### Linux
* **Distribuição Alvo:** Linux Mint 22 (Wilma) / Ubuntu 24.04 LTS (Noble Numbat) amd64.
* **Dependências de Sistema:** `ffmpeg`, `libssl3` / `libssl-dev`, `qt6-base-dev`.
* **Acesso à GPU:** Dispositivo `/dev/dri/renderD128` acessível pelo usuário (grupo `render` ou `video`).

---

> [!NOTE]
> Para consultar os guias específicos, clique nos links da tabela acima ou navegue diretamente pelos arquivos na pasta `docs/`.
