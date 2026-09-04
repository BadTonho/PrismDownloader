# Registro de mudanças — versão planejada 2.0.3

> Este arquivo é apenas um registro de trabalho. A versão oficial do projeto não foi alterada.

## Correções e melhorias registradas

- Corrigida a inconsistência entre o formato exibido na seleção e o formato realmente enviado ao `yt-dlp`.
- Os IDs exatos dos fluxos de vídeo e áudio agora são preservados desde a leitura dos metadados até o download individual.
- A estimativa de tamanho passa a ficar associada aos mesmos fluxos selecionados para o download.
- Em downloads individuais, o seletor exato de vídeo + áudio é utilizado.
- Em playlists e lotes, permanece o fallback por resolução para que cada item escolha seus próprios fluxos válidos.
- Aumentada a concorrência de fragmentos do `yt-dlp` de 4 para 8 para melhorar a taxa de transferência.
- Adicionada recuperação automática quando a taxa de download fica abaixo de 100 KB/s por throttling forte.

## Validação realizada

- Projeto compilado com sucesso em modo Release.
- Todos os 7 testes automatizados passaram.
- Adicionado teste para confirmar o envio do seletor exato de vídeo e áudio.

## Pendências antes do lançamento

- Validar a velocidade em novos downloads reais do YouTube.
- Confirmar estabilidade com vídeos individuais e playlists.
- Só alterar a versão oficial do projeto quando os testes estiverem concluídos.
