# Sistema de Atualização Automática — Prism Downloader

Este documento descreve os fluxos de atualização do aplicativo Prism
Downloader e do motor `yt-dlp`.

## Validação da atualização do aplicativo

Cada release publica um `prism-update-manifest.json` com a versão e os hashes
SHA-256 dos pacotes Setup, Portátil e DEB. O aplicativo baixa esse manifesto,
confere se ele corresponde à tag da release no GitHub e valida o pacote baixado
contra o SHA-256 declarado antes de instalar.

Uma release precisa destes arquivos:

- `PrismDownloader_vX.Y.Z_Setup.exe`
- `PrismDownloader_vX.Y.Z_Portable.zip`
- `prism-downloader_X.Y.Z_amd64.deb`
- `prism-update-manifest.json`

O manifesto é gerado com `scripts/create_update_manifest.py`:

```powershell
python scripts/create_update_manifest.py --version 2.0.1 --output dist/prism-update-manifest.json `
  --asset PrismDownloader_v2.0.1_Setup.exe=dist/PrismDownloader_v2.0.1_Setup.exe `
  --asset PrismDownloader_v2.0.1_Portable.zip=dist/PrismDownloader_v2.0.1_Portable.zip `
  --asset prism-downloader_2.0.1_amd64.deb=dist/prism-downloader_2.0.1_amd64.deb
```

O manifesto verifica a integridade do download, mas não garante sozinho a
autenticidade da publicação. Use a página oficial de releases do GitHub e
confira os arquivos antes de instalar.

## Fluxo por tipo de pacote

### Windows instalado pelo Inno Setup

O aplicativo baixa `PrismDownloader_vX.Y.Z_Setup.exe`, valida o SHA-256,
executa o instalador e reinicia.

### Windows portátil

O aplicativo baixa e valida o ZIP Portátil e executa o
`portable-update-helper.exe` para substituir os arquivos depois que o processo
atual for encerrado. O helper continua mantendo o rollback existente.

### Linux

O aplicativo baixa e valida `prism-downloader_X.Y.Z_amd64.deb` e solicita a
instalação pela ferramenta de pacotes do sistema.

## Atualizações independentes do `yt-dlp`

O atualizador do `yt-dlp` continua usando o arquivo oficial `SHA2-256SUMS` para
validar o binário baixado. Esse processo é independente do manifesto do
aplicativo.
