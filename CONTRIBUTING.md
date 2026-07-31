# 🤝 Como Contribuir para o Prism Downloader

Obrigado por demonstrar interesse em colaborar com o **Prism Downloader**, um projeto idealizado pela **Tonho Studios** para redefinir a performance de extração de mídias no Windows!

Seja você um programador experiente em C++, um designer de interfaces em Qt ou um testador de plataforma, toda ajuda é incrivelmente bem-vinda!

---

## 📋 Como Abertura de Issues (Relatório de Bugs e Sugestões)

Ao encontrar um comportamento anormal ou desejar sugerir uma funcionalidade:
1. Verifique na nossa lista de [Issues Permanentes](https://github.com/BadTonho/Baixar/issues) se alguém já reportou o mesmo tema.
2. Utilize títulos claros, por exemplo: `[Bug] Janela piscando no Windows Terminal preview 1.18` ou `[Sugestão] Atalho de teclado para limpar biblioteca`.
3. Anexe capturas de tela e o log de execução sempre que possível.

---

## 🛠️ Normas para Submissão de Pull Requests (PRs)

Se você alterou o código-fonte C++ ou aprimoration a interface Qt e deseja enviar um Pull Request:

1. **Padrão de Linguagem:** Mantenha o código em **C++17 puro**. Não utilize bibliotecas de terceiros pesadas quando a STL (`std::`) ou o ecossistema nativo do **Qt 6** já solucionarem o problema.
2. **Regra de Ouro do Kernel:** Ao instanciar qualquer `QProcess` que invoque binários externos no Windows, **é estritamente obrigatório** aplicar o modificador de criação com a flag `0x08000000` (`CREATE_NO_WINDOW`) para impedir que janelas do terminal pisquem na tela do usuário.
3. **Sem Dados Pessoais:** Realize um `git status` e verifique se pastas locais, logs do Visual Studio ou arquivos grandes (`.exe`/`.dll`) não foram adicionados acidentalmente ao seu commit.
4. **Resgate de Conflitos:** Sempre mantenha o seu *fork* sincronizado com o branch `main` oficial antes de abrir o PR.

> *Agradecemos pelo seu tempo, código e dedicação em construir um software robusto e limpo conosco!*
