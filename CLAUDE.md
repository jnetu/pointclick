# CLAUDE.md — pointclick

Engine/jogo point & click 2D em C++20 com SDL3 (3.4.x), alvo PC, celular, tablet e talvez console.

**Para qualquer trabalho de arquitetura, sistema novo, CMake, performance ou multiplataforma, carregue e siga a skill `cpp-sdl-modular-architecture`** (Modo A, seção 10: point & click). Não introduza ECS.

## Contratos e mapa do projeto

@AGENTS.md

Leia também `docs/ARCHITECTURE.md` antes de mudanças estruturais e `docs/GAME_DESIGN.md` antes de mexer nos formatos `.room`/`.rules`.

## Dívidas conhecidas (não replique; corrija quando tocar na área)

- Nenhuma registrada no momento. A lista de pendências priorizadas fica em `docs/ROADMAP.md`.

## Build e testes

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Os presets `debug`/`release` usam o gerador padrão da plataforma (ou `CMAKE_GENERATOR`);
`ninja-debug`/`ninja-release` fixam Ninja. A build do CLion fica em `cmake-build-debug`
(`cmake --build cmake-build-debug && ctest --test-dir cmake-build-debug --output-on-failure`).
SDL3: uma versão de referência (`POINTCLICK_SDL3_VERSION` em `cmake/Dependencies.cmake`)
vale para o pacote do sistema (mínimo) e para o CPM. Estilo: `.clang-format` calibrado com o
clang-format **21** (major fixado; `pip install "clang-format==21.*"`), aplicado só nas linhas
alteradas: `cmake --build --preset debug --target format-check` (ou `format` para corrigir).
Nunca reformate arquivos inteiros. Lógica dos alvos em `cmake/ClangFormat.cmake`.
Para rodar com os assets da raiz: `pointclick --rooms assets/rooms`.
Validar o conteúdo sem abrir janela (salas, `.rules`, `.dlg`, `change_room`; sai ≠ 0 com erro;
arte ausente é só `aviso:` e não muda o código de saída):
`pointclick --rooms assets/rooms --check` (também roda no `ctest`). Testar do zero sem mexer no
save: `--new-game --no-save`; começar numa sala: `--room <sala> [spawn]` (jogo novo).
Parsing em `src/core/CommandLine.*` (sem SDL), validação em `src/content/ContentCheck.*`.
Build sem o editor (como numa loja): `-DPOINTCLICK_ENABLE_TOOLS=OFF`.

Antes de concluir: checklist da skill (seção 12) e todos os testes passando.

## Delegacao ao Argon (agy)

Voce e o arquiteto/revisor. Para economizar tokens, trabalho volumoso e bem definido vai para o Argon via o subagente `argon-delegate` (que usa `scripts/argon.sh`). Config: `ARGON_MODEL` (veja `agy models`), `ARGON_EFFORT`, `ARGON_TIMEOUT`.

**Delegue** somente se a tarefa tocar 3+ arquivos ou for em lote: investigacao de modulos, refatoracao mecanica, testes de unidade/regressao, documentacao, implementacao de tarefa ja especificada.
**Nao delegue**: decisoes de camada/arquitetura, mudanca de formato `.room`/`.rules`, catalogos (`RoomParameters`, catalogo de acoes), depuracao complexa, tarefas pequenas (fazer direto e mais barato que briefing + revisao).

Fluxo: 1) planejar e dividir; 2) briefing autocontido com escopo, arquivos proibidos e criterios de aceite; 3) cada tarefa roda em branch `argon/<nome>` num worktree proprio (o Argon so ve commits, nao mudancas locais); 4) revisar por `git diff --stat` e leitura apenas dos trechos criticos, nunca o diff inteiro; 5) rodar build e testes voce mesmo; 6) pedir correcao com novo briefing curto ou corrigir direto se for pequeno; 7) integrar (merge) so apos aprovar.

Nunca confie cegamente no relatorio do Argon. Nunca integre sem rodar os testes.
