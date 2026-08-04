# Análise técnica — Prism Downloader

Data da análise: 04/08/2026  
Escopo: revisão estática do código-fonte, CMake, instalador e artefatos locais. Não foram feitos downloads de mídia nem chamadas à API do GitHub.

## Status das correções — 04/08/2026

As seguintes correções foram implementadas após esta análise:

- O aplicativo não baixa nem executa mais instaladores ou atualiza o `yt-dlp` automaticamente. Para novas versões, abre somente a página oficial no navegador; a verificação de versão agora é semântica e rejeita URL/resposta inválida.
- O download usa `QProcess::start(program, arguments)` com `--` antes da URL; URL, intervalo e diretório são validados. O intervalo é aceito apenas como `HH:MM:SS-HH:MM:SS` com início menor que o fim.
- Os perfis 4K/1080p/720p agora geram seletores distintos do `yt-dlp`, cobertos por teste unitário.
- O caminho final emitido pelo `yt-dlp` é propagado à interface, eliminando a seleção do arquivo “mais recente” para conversão automática e para a fila.
- A conversão usa o encoder do fabricante correto (NVENC/AMF/QSV), tenta fallback seguro para CPU se o encoder de hardware falhar e não sobrescreve arquivos existentes.
- O cancelamento de download encerra a árvore de processos no Windows; logs são texto simples e têm limite de 5.000 entradas.
- CMake usa UTF-8, avisos elevados e caminho de Qt configurável. O instalador falha se FFmpeg/yt-dlp não estiverem presentes. Há teste CTest para a seleção de perfil.

Pendências deliberadas: a atualização integrada só deve voltar quando houver uma infraestrutura de assinatura verificável (chave pública/assinatura de release ou validação Authenticode com certificado fixado). Também faltam testes de integração com `yt-dlp`/FFmpeg reais e paginação assíncrona da biblioteca para pastas muito grandes.

## Resumo executivo

O projeto compila no estado atual, mas há riscos importantes antes de uma distribuição ampla: o atualizador executa instaladores sem verificar autenticidade, a conversão falha em máquinas AMD/Intel apesar de anunciá-las como aceleradas, a escolha de qualidade não é aplicada ao `yt-dlp` e a conversão pode sobrescrever arquivos existentes sem confirmação.

Prioridade recomendada:

1. Blindar ou desabilitar temporariamente a instalação automática de atualizações.
2. Corrigir a seleção de codec por fabricante/capacidade e a escolha real de formato do download.
3. Impedir sobrescrita de arquivos e usar caminhos/argumentos tipados, sem montar comandos como texto.
4. Tornar o empacotamento e a cobertura de testes reproduzíveis.

## Validações realizadas

- `cmake --build build --config Release --parallel 2`: executado com sucesso (build incremental).
- `ctest --test-dir build -N`: **0 testes** configurados.
- Os binários `yt-dlp.exe`, `ffmpeg.exe`, DLLs Qt e `platforms/qwindows.dll` existem no artefato local `build/Release`.
- A árvore Git estava limpa no início da análise.

Uma compilação incremental bem-sucedida não substitui testes funcionais de download, conversão, cancelamento e atualização.

## Achados prioritários

### P0 — Atualizador baixa e executa código sem verificação de integridade/autoria

**Evidência:** [`src/MainWindow.cpp`](src/MainWindow.cpp), linhas 1798–1926.

O aplicativo obtém a URL de um asset de release, baixa todo o conteúdo e, se o arquivo terminar em `.exe`, chama `QProcess::startDetached(savePath, ...)`. HTTPS protege o transporte, mas não comprova que o arquivo é o instalador publicado e assinado pela aplicação. Uma conta, release, cadeia de entrega ou arquivo comprometido pode resultar na execução de código malicioso após a confirmação do usuário.

**Correção recomendada:**

- Publicar e validar uma assinatura criptográfica do manifesto/instalador (por exemplo, assinatura Ed25519 com chave pública embutida) antes de disponibilizar a instalação.
- Assinar o instalador com Authenticode e validar explicitamente o certificado esperado antes de iniciá-lo.
- Restringir URLs a `https`, validar redirecionamentos e o nome do asset como nome-base seguro.
- Enquanto não houver validação, trocar o fluxo por abertura da página oficial de releases no navegador, sem baixar/executar o binário pelo app.

### P1 — Conversão anunciada para AMD/Intel usa NVENC e falhará nesses equipamentos

**Evidência:** [`src/GPUDetector.cpp`](src/GPUDetector.cpp), linhas 54–72; [`src/MainWindow.cpp`](src/MainWindow.cpp), linhas 50–60 e 872–886.

`GPUDetector` identifica AMD e Intel e informa codecs AMF/QSV, porém `onStartConvertClicked()` testa apenas `hasHardwareAcceleration()` e sempre seleciona `h264_nvenc`/`hevc_nvenc`. Além de a conversão falhar em AMD/Intel, a interface sempre exibe “NVENC”. A detecção por nome do adaptador também não prova que o encoder esteja disponível no FFmpeg/driver instalado.

**Correção recomendada:** selecionar os argumentos por `GPUType` ou pelo codec recomendado; testar a capacidade real com `ffmpeg -encoders` (e, idealmente, um teste curto controlado), mantendo fallback automático para CPU em caso de falha.

### P1 — Perfis 4K, 1080p e 720p não alteram o formato baixado

**Evidência:** [`src/DownloadEngine.cpp`](src/DownloadEngine.cpp), linhas 48–85.

O perfil escolhido é guardado em `MediaItem`, mas qualquer opção de vídeo executa o mesmo seletor: `-f "bestvideo+bestaudio/best"`. Assim, as opções 4K/1080p/720p são somente visuais; a qualidade final depende da melhor stream disponível e pode contrariar a escolha do usuário.

**Correção recomendada:** mapear perfis para seletores explícitos do `yt-dlp` (por exemplo, limite de altura para 1080p/720p), informar o fallback escolhido e testar o mapeamento de formatos isoladamente.

### P1 — Conversão sobrescreve arquivos existentes silenciosamente

**Evidência:** [`src/MainWindow.cpp`](src/MainWindow.cpp), linhas 861–903.

O argumento `-y` combinado com nomes determinísticos como `arquivo_convertido.mp4` substitui qualquer arquivo homônimo sem pergunta. Isso causa perda de dados quando o usuário reconverte um arquivo ou quando há colisão de nomes.

**Correção recomendada:** usar `-n` por padrão, gerar um nome único (`QTemporaryFile`/sufixo incremental) ou exibir um diálogo de substituição explícito. Escrever primeiro em arquivo temporário e renomear somente após sucesso também evita deixar um destino parcialmente corrompido.

### P1 — Conversão automática pode operar sobre o arquivo errado

**Evidência:** [`src/MainWindow.cpp`](src/MainWindow.cpp), linhas 110–155.

Depois do download, o app escolhe o primeiro item de `entryInfoList(..., QDir::Time)`, ou seja, o arquivo mais recente da pasta. Se a pasta já tiver mídia mais nova, outro download ou um arquivo recém-alterado, a conversão automática pode usar um arquivo não relacionado.

**Correção recomendada:** fazer o `yt-dlp` emitir o caminho final da mídia (por exemplo com `--print after_move:filepath`), propagar esse resultado estruturado pelo `DownloadEngine` e passar exatamente esse caminho ao conversor.

### P1 — Argumentos externos são montados como uma única string

**Evidência:** [`src/DownloadEngine.cpp`](src/DownloadEngine.cpp), linhas 60–85 e 119–124.

Embora não use `cmd.exe` nesse fluxo, `startCommand()` precisa interpretar uma string que contém URL, diretório e intervalo de tempo vindos da interface. Aspas nesses valores podem alterar a separação de argumentos e injetar opções no `yt-dlp`; o intervalo de tempo também não é validado. Isso é menos grave que injeção de shell, mas ainda torna o comportamento inseguro e frágil.

**Correção recomendada:** usar `QProcess::start(program, QStringList arguments)`, acrescentar `--` antes da URL e validar o intervalo com formato estrito (por exemplo, `HH:MM:SS-HH:MM:SS`, início menor que fim). Não registrar URLs completas caso contenham tokens sensíveis.

### P1 — Processo filho pode permanecer ativo após “Cancelar”

**Evidência:** [`src/DownloadEngine.cpp`](src/DownloadEngine.cpp), linhas 138–147; [`src/MainWindow.cpp`](src/MainWindow.cpp), linhas 937–947 e 1387–1402.

`QProcess::kill()` encerra apenas o processo diretamente iniciado. `yt-dlp` pode iniciar FFmpeg; nesse caso, cancelar o processo pai não garante encerrar a árvore de processos nem limpar arquivos temporários.

**Correção recomendada:** no Windows, executar cada operação em Job Object com `KILL_ON_JOB_CLOSE` (ou outro mecanismo confiável de árvore de processos), solicitar encerramento normal antes de forçar a parada e limpar arquivos parciais de forma segura.

### P1 — O pacote de distribuição não é reproduzível pelo CMake

**Evidência:** [`CMakeLists.txt`](CMakeLists.txt), linhas 7–55; [`setup_script.iss`](setup_script.iss), linhas 31–32.

O CMake copia somente parte do Qt e não copia/declara `yt-dlp.exe` nem `ffmpeg.exe`. Eles estão no `build/Release` local, mas uma compilação limpa do repositório não os produzirá. O instalador apenas empacota tudo que já estiver nessa pasta. Além disso, caminhos de Qt 6.7.2 e DLLs são fixos, mesmo que outro Qt tenha sido encontrado pelo CMake.

**Correção recomendada:** criar regras `install()`/CPack ou uma etapa de empacotamento versionada que obtenha/valide os motores por checksum, use `windeployqt` para as dependências Qt e falhe se qualquer binário obrigatório estiver ausente. Centralizar versão e caminho de Qt em opções configuráveis.

## Achados relevantes (P2)

### Atualizador aceita qualquer versão diferente, inclusive downgrade

**Evidência:** [`src/MainWindow.cpp`](src/MainWindow.cpp), linhas 1773–1788.

O código compara apenas igualdade de texto. Uma tag remota menor ou inválida é tratada como “nova versão”. Use `QVersionNumber`, rejeite versões menores/ambíguas e defina política explícita para pré-releases. Resposta 404 ou JSON inválido também não deve ser apresentada como confirmação de que a instalação está atualizada.

### Download de atualização inteiro em memória e sem limites/timeout próprios

**Evidência:** [`src/MainWindow.cpp`](src/MainWindow.cpp), linhas 1872–1911.

`readAll()` carrega o instalador completo em RAM antes de gravá-lo. Arquivos grandes podem degradar ou encerrar o aplicativo; não há limite de tamanho, verificação de espaço disponível ou timeout por requisição. Grave em `QSaveFile` durante `readyRead`, imponha limite esperado, monitore espaço e remova o temporário em erro/cancelamento.

### Biblioteca e fila não preservam o diretório individual do download

**Evidência:** [`src/MainWindow.cpp`](src/MainWindow.cpp), linhas 1045–1078 e 1109–1130.

A biblioteca sempre lista `m_outputDirInput` (diretório padrão) e o duplo clique da fila também usa essa pasta. Um arquivo salvo no destino temporário escolhido no modal pode não aparecer na biblioteca ou não abrir pela fila. Guarde o caminho final em cada item da fila, em vez de recompor o caminho pelo nome.

### Logs ilimitados degradam memória e interface

**Evidência:** [`src/MainWindow.cpp`](src/MainWindow.cpp), linhas 1404–1475.

`m_allLogs` cresce sem teto e cada mensagem é adicionada a `QTextEdit`. Downloads longos, playlists ou saídas verbosas podem consumir memória e deixar a UI lenta; refiltrar reprocessa todo o histórico. Limite por linhas/bytes, descarte em bloco as entradas antigas, use `QPlainTextEdit` e trate saída externa como texto simples.

### Varredura de diretório e `QTableWidget` podem bloquear a UI

**Evidência:** [`src/MainWindow.cpp`](src/MainWindow.cpp), linhas 1045–1078.

A enumeração e criação de um item por arquivo ocorrem na thread de interface toda vez que a biblioteca é atualizada. Em pastas grandes isso causa travamentos perceptíveis. Mova a leitura para worker, imponha paginação/filtro e prefira model/view (`QTableView` + modelo) a `QTableWidget` para escala maior.

### Progresso da conversão não representa percentual real

**Evidência:** [`src/MainWindow.cpp`](src/MainWindow.cpp), linhas 910–914 e 950–974.

A barra fica indeterminada e o status extrai um trecho textual do stderr. Use `ffmpeg -progress pipe:2 -nostats`, obtenha a duração do input com `ffprobe` e calcule o percentual a partir de `out_time_ms`.

### Codificação dos fontes pode corromper textos em alguns ambientes MSVC

**Evidência:** arquivos C++ em UTF-8 sem BOM com acentos/emojis; não há `/utf-8` em [`CMakeLists.txt`](CMakeLists.txt).

Os arquivos são UTF-8 válidos, mas o CMake não fixa a codificação de entrada/execução. Em ambientes MSVC cuja página de código padrão não seja UTF-8, labels e logs podem ser compilados incorretamente. Adicione `/utf-8` para MSVC e padronize UTF-8 no editor/CI.

### Ausência de testes, analisadores e warnings rigorosos

**Evidência:** `ctest -N` reportou 0 testes; não há configuração de `add_test`, warnings elevados ou análise estática no CMake.

Cobrir pelo menos: montagem de argumentos do `yt-dlp`, parser de progresso, versões, intervalo de tempo, seleção de codec/fallback, colisão de saída e caminho retornado pelo download. No CI, habilite `/W4` (ou equivalente), trate warnings novos como erro e execute `clang-tidy`/sanitizers onde aplicável.

## Melhorias de arquitetura e manutenção (P3)

- Separar UI, orquestração de downloads, conversão e atualização em serviços testáveis; `MainWindow.cpp` concentra ~2.000 linhas e múltiplas responsabilidades.
- Substituir strings de perfil por um tipo estruturado (`id`, seletor do yt-dlp, extensão, codec, fallback), evitando decisões por `startsWith()`.
- Remover `GPUDetector::execCommand()` ou implementá-lo com tratamento de erro; no estado atual ele é código morto e executa `cmd.exe` caso seja reutilizado futuramente.
- Centralizar a versão em uma única fonte gerada para CMake, aplicativo e Inno Setup. Atualmente C++ e `.iss` precisam ser mantidos manualmente.
- Atualizar o roadmap: ele ainda cita `NeoVDownloader` e artefatos `V1.0`, enquanto o produto e release atuais são Prism Downloader 1.1.5.
- Adicionar `*.log` ao `.gitignore` ou justificar o versionamento de `aqtinstall.log`; logs de instalação tendem a introduzir ruído e caminhos locais no repositório.

## Roteiro sugerido de correção

1. **Segurança e dados:** impedir sobrescrita, trocar `startCommand` por programa + lista de argumentos, validar entrada e bloquear execução de atualização não verificada.
2. **Confiabilidade funcional:** aplicar perfis de qualidade de verdade, corrigir AMD/Intel/fallback e transportar o caminho final real do download.
3. **Distribuição:** tornar o bundle de Qt/FFmpeg/yt-dlp declarativo e validado em build limpo; assinar releases.
4. **Qualidade contínua:** criar testes de unidade/integração com executáveis simulados e CI com warnings/análise estática.
5. **Escala de UX:** limitar logs, tornar biblioteca assíncrona e implementar progresso real de conversão.
