# Registro de mudanças — versão planejada 2.0.3

> Este arquivo é apenas um registro de trabalho. A versão oficial do projeto não foi alterada.

## Correções e melhorias registradas

- Adicionado um seletor de faixa de áudio ao diálogo de formatos, com idioma, codec e bitrate.
- O seletor preserva todas as faixas de áudio distintas e inicia na faixa indicada como preferida pelo `yt-dlp`, quando essa informação está disponível.
- Em downloads individuais, o seletor combina os IDs exatos do formato de vídeo e da faixa de áudio escolhida.
- Em playlists e lotes, a seleção usa o idioma escolhido em cada item e recorre à faixa padrão da fonte quando esse idioma não está disponível.
- A seleção de áudio também é aplicada a downloads em MP3; formatos com áudio embutido que não pode ser substituído desabilitam essa opção.
- Os detalhes do formato e as estimativas de tamanho são atualizados quando a faixa de áudio selecionada muda.
- Corrigida a inconsistência entre o formato exibido na seleção e o formato realmente enviado ao `yt-dlp`.
- Aumentada a concorrência de fragmentos do `yt-dlp` de 4 para 8 para melhorar a taxa de transferência.
- Adicionada recuperação automática quando a taxa de download fica abaixo de 100 KB/s por throttling forte.

## Validação realizada

- Compilação Windows em modo Release concluída com sucesso.
- Todos os 8 testes automatizados passaram, incluindo testes do seletor visual, dos metadados de áudio e dos argumentos enviados ao `yt-dlp`.

## Pendências antes do lançamento

- Validar a velocidade em novos downloads reais do YouTube.
- Confirmar estabilidade com vídeos individuais e playlists.
- Validar a compilação e os testes no Linux.
- Só alterar a versão oficial do projeto quando os testes estiverem concluídos.
