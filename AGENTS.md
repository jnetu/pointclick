# Mapa rápido do projeto

Leia este arquivo antes de alterar o jogo. Consulte `docs/ARCHITECTURE.md` para
os contratos e `docs/GAME_DESIGN.md` para o formato editável das salas.

| Mudança | Comece por | Depois confira |
| --- | --- | --- |
| Parâmetro de sala | `src/game/Room.hpp`, `src/game/RoomParameters.cpp` | `Room::validationError`, `docs/GAME_DESIGN.md` |
| Chave nova do formato `.room` | `src/content/RoomFiles.cpp` | `tests/EditorTests.cpp` |
| Campo de corte de spritesheet | `src/game/SpriteClip.hpp`, `src/content/SpriteFields.cpp` | `src/graphics/SpriteLibrary.cpp`, `tests/SpriteTests.cpp` |
| Novo clip de player ou objeto | `assets/rooms/*.room` | `assets/sprites`, `docs/GAME_DESIGN.md`; padrões em `src/content/RoomPresets.cpp` |
| Regra que escolhe animação | `src/game/Animation.*`, `src/game/ObjectAnimations.*` ou `src/game/Player.*` | `src/graphics/EntityRenderer.cpp` apenas se mudar a apresentação |
| Nova direção de locomoção | `PlayerPose` e `playerLocomotionAnimations` em `src/game/Player.hpp` | regra em `Player.cpp` e clip no `.room` |
| Movimento ou obstáculo | `src/game/Navigation.*`, `src/game/Game.cpp` | `tests/GameTests.cpp` |
| Câmera e dimensões da sala | `src/game/Camera.*`, `src/game/Room.*`, `src/game/RoomParameters.cpp` | `src/graphics/SceneRenderer.cpp`, `tests/GameTests.cpp`, `tests/EditorTests.cpp` |
| Aparência da sala | `src/graphics/RoomBackdropRenderer.cpp` | `src/game/Room.hpp` |
| Camadas e composição | `src/graphics/SceneRenderer.cpp` | âncoras em `SceneObject` e `SpriteClip` |
| Comandos DEBUG e área do painel | `src/debug/DebugEditor.*` | `src/graphics/DebugOverlay.cpp`, `tests/EditorTests.cpp` |
| Entrada multiplataforma e roteamento UI | `src/input/InputSystem.*`, `src/input/InputRouter.*` | `src/core/Application.cpp`, `src/graphics/Renderer.cpp`, `tests/EditorTests.cpp` |
| Regras de interação, itens e puzzles | `assets/rooms/*.rules`, `src/game/Interaction.*`, `src/content/InteractionFiles.*` | `src/game/Game.cpp`, `tests/GameTests.cpp`, `tests/EditorTests.cpp` |
| Progresso de partida (flags, inventário, overrides) | `src/game/SessionState.*` | `src/game/Game.cpp`, `src/graphics/SceneRenderer.cpp`, `tests/GameTests.cpp` |
| UI de Diálogo / Inventário futuro | novo módulo UI consumindo `SessionState` / `Game::useItemOnObject` | `ViewportUiConsumer` em `src/input/InputRouter.hpp`, interface em `graphics/` |
| Efeitos visuais de transição de sala | `Game::consumeRoomTransition()` em `src/core/Application.cpp` | efeito visual temporizado em `graphics/` |

## Contratos

- `Room`, `SceneObject`, `SpriteClip`, `AnimationSet` e `RoomInteractions` são
  definições autoradas/editáveis. Não guarde neles progresso de diálogo, itens
  coletados, flags ou tempo corrente.
- `SessionState` guarda o progresso persistente da sessão (flags/contadores,
  itens de inventário e overrides por `roomId + objectId` de visibilidade e
  colisão `solidOverride`). Editar a sala em tempo real no `DebugEditor`
  (`Game::applyRoom(room, false, error)`) ou trocar de sala **nunca** apaga o
  `SessionState`.
- `World::width` × `World::height` (`1280 × 720`) definem o campo de visão fixo da
  câmera/viewport lógica; o tamanho total da cena pertence a `Room::width` e
  `Room::height`. UI e overlays usam coordenadas de viewport; o mundo usa coordenadas
  da sala deslocadas por `Camera`.
- `Game`, `Player`, `Camera`, `PendingInteraction` e `AnimationPlayback` guardam
  estado de execução da cena. Regras de gameplay não devem carregar texturas nem
  fazer chamadas de desenho.
- `input/` traduz eventos brutos de mouse/teclado, toque (ignorando mouse sintético)
  e gamepad (hotplug, eixo analógico com *deadzone* e cursor virtual) para a viewport
  lógica, roteando por prioridade (`Ciclo de Vida → DebugEditor → ViewportUiConsumer → Game`)
  sem depender de `graphics/`.
- `graphics/` lê o estado do jogo (`const Game&`, respeitando `Game::isObjectVisible`
  e `Game::isObjectSolid`) e desenha. Não decide resultados de interação,
  hit-test de UI, colisão, diálogo ou transição de sala.
- `content/` traduz arquivos `.room` (estética/espaço) e arquivos companheiros
  opcionais `.rules` (regras declarativas de interação) em definições validadas por
  linha e de volta para arquivos. Preserve a leitura de chaves antigas ao evoluir.
- `debug/` altera a definição da sala para teste e define sua própria geometria de
  captura na viewport (`DebugEditor::panelRect`), preservando `SessionState`.
  Não é o sistema de gameplay.
- IDs de objetos, itens, flags e nomes de animação são estáveis, com letras,
  números, `_` e `-`. Use esses nomes nas regras, sem depender da posição no vetor da sala.

## Verificar mudanças

```sh
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

Há também uma build local em `cmake-build-debug`, caso já esteja configurada.
Para testar arquivos da raiz do projeto, execute o jogo com `--rooms assets/rooms`.
