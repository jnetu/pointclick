# CLAUDE.md — pointclick

Engine/jogo point & click 2D em C++20 com SDL3 (3.4.x), alvo PC, celular, tablet e talvez console.

**Para qualquer trabalho de arquitetura, sistema novo, CMake, performance ou multiplataforma, carregue e siga a skill `cpp-sdl-modular-architecture`** (Modo A, seção 10: point & click). Não introduza ECS.

## Contratos e mapa do projeto

@AGENTS.md

Leia também `docs/ARCHITECTURE.md` antes de mudanças estruturais e `docs/GAME_DESIGN.md` antes de mexer nos formatos `.room`/`.rules`.

## Dívidas conhecidas (não replique; corrija quando tocar na área)

- `input/InputSystem.hpp` inclui `debug/DebugEditor.hpp`: input deve emitir ações abstratas.
- `debug/` e `input/` estão dentro de `pointclick_core`; o alvo é um target por camada e ferramentas opcionais.
- Tick a 20 Hz e tempo de animação sem interpolação.
- Cache de texturas limpo a cada `roomRevision`; `SpriteLibrary::draw` valida o clip a cada desenho.
- Rota, cruz de destino e diagnóstico desenhados em produção (devem ir para flags de debug).
- `path.string()` em `SpriteLibrary` e nos `save` (usar UTF-8); gravação via `std::ofstream`, sem `SDL_GetPrefPath`.
- Arte do player repetida por `.room`; `gallery.room` depende em silêncio de `RoomPresets`.

## Build e testes

```sh
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

O preset usa Ninja; se não estiver no `PATH`, use a build do CLion em `cmake-build-debug`
(`cmake --build cmake-build-debug && ctest --test-dir cmake-build-debug --output-on-failure`).
Para rodar com os assets da raiz: `pointclick --rooms assets/rooms`.

Antes de concluir: checklist da skill (seção 12) e todos os testes passando.

## Delegacao ao Argon (agy)

Voce e o arquiteto/revisor. Para economizar tokens, trabalho volumoso e bem definido vai para o Argon via o subagente `argon-delegate` (que usa `scripts/argon.sh`). Config: `ARGON_MODEL` (veja `agy models`), `ARGON_EFFORT`, `ARGON_TIMEOUT`.

**Delegue** somente se a tarefa tocar 3+ arquivos ou for em lote: investigacao de modulos, refatoracao mecanica, testes de unidade/regressao, documentacao, implementacao de tarefa ja especificada.
**Nao delegue**: decisoes de camada/arquitetura, mudanca de formato `.room`/`.rules`, catalogos (`RoomParameters`, catalogo de acoes), depuracao complexa, tarefas pequenas (fazer direto e mais barato que briefing + revisao).

Fluxo: 1) planejar e dividir; 2) briefing autocontido com escopo, arquivos proibidos e criterios de aceite; 3) cada tarefa roda em branch `argon/<nome>` num worktree proprio (o Argon so ve commits, nao mudancas locais); 4) revisar por `git diff --stat` e leitura apenas dos trechos criticos, nunca o diff inteiro; 5) rodar build e testes voce mesmo; 6) pedir correcao com novo briefing curto ou corrigir direto se for pequeno; 7) integrar (merge) so apos aprovar.

Nunca confie cegamente no relatorio do Argon. Nunca integre sem rodar os testes.
