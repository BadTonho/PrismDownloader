# Regras Operacionais e Comportamentais do Agente (Prism Downloader)

## 1. Regra para Compilação do Instalador e Empacotamento
* **NUNCA compilar ou gerar instaladores por conta própria (autonomia zero):** O agente não deve acionar o Inno Setup (`ISCC.exe` ou `setup_script.iss`) ou compactar pacotes para a pasta `dist/` (`_Setup.exe` ou `_Portable.zip`) sem autorização ou por iniciativa própria.
* **Compilação sob demanda autorizada:** A geração de pacotes finais e do instalador é permitida **apenas quando requisitada explicitamente com um comando direto do usuário** (ex: "compile o instalador", "gere o empacotamento").
* **Compilação de teste permitida:** O agente permanece autorizado a executar compilações rápidas do binário simples de teste de desenvolvimento (`build/Release/PrismDownloader.exe` via CMake) para validação diária de código.

## 2. Gestão de Versionamento (Git)
* **Zero interferência remota no Git:** É expressamente proibido executar comandos como `git push`, `git pull` ou tentar gerenciar sincronização de repositórios remotos no GitHub. Todas as operações de push/pull são realizadas manualmente pelo usuário.

## 3. Respostas Diretas e Moderação de Prontidão (Zero Proatividade Agressiva)
* **Responder estritamente ao que foi perguntado:** Quando o usuário fizer perguntas informativas (por exemplo, "você fez X?", "isso foi compilado?"), o agente deve limitar-se a responder com os fatos (Sim/Não e breve explicação do estado anterior), **sem acionar ferramentas, sem alterar arquivos e sem iniciar processos não solicitados**.
