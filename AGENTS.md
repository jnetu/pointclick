# Mapa rápido do projeto

Leia este arquivo antes de alterar o jogo. Consulte `docs/ARCHITECTURE.md` para
os contratos e `docs/GAME_DESIGN.md` para o formato editável das salas.

| Mudança | Comece por | Depois confira |
| --- | --- | --- |
| Parâmetro de sala | `src/game/Room.hpp`, `src/game/RoomParameters.cpp` | `Room::validationError`, `docs/GAME_DESIGN.md` |
| Chave nova do formato `.room` | `src/content/RoomFiles.cpp` | `tests/EditorTests.cpp` |
| Campo de corte de spritesheet | `src/game/SpriteClip.hpp`, `src/content/SpriteFields.cpp` | `src/graphics/SpriteLibrary.cpp`, `tests/SpriteTests.cpp` |
| Novo clip de player | `assets/actors/player.actor` (ou `player.<clip>.*` numa sala, para sobrescrever) | `assets/sprites`, `docs/GAME_DESIGN.md`; `src/content/ActorFiles.*`, `src/content/RoomLibrary.*` |
| Novo clip de objeto | `assets/rooms/*.room` | `assets/sprites`, `docs/GAME_DESIGN.md` |
| Regra que escolhe animação | `src/game/Animation.*`, `src/game/ObjectAnimations.*` ou `src/game/Player.*` | `src/graphics/EntityRenderer.cpp` apenas se mudar a apresentação |
| Nova direção de locomoção | `PlayerPose` e `playerLocomotionAnimations` em `src/game/Player.hpp` | regra em `Player.cpp` e clip no `.room` |
| Ponto de entrada (`[spawn]`) e `change_room <sala> [spawn]` | `RoomSpawn`/`Room::entryPoint` em `src/game/Room.*`, `Scene::apply` | `src/content/RoomFiles.cpp`, `RoomLibrary::transitionsError`, `Application::processGameEvents`, `tests/GameTests.cpp`, `tests/EditorTests.cpp` |
| Movimento ou obstáculo | `src/game/Navigation.*`, `src/game/Scene.cpp` | `tests/GameTests.cpp` |
| Câmera e dimensões da sala | `src/game/Camera.*`, `src/game/Room.*`, `src/game/RoomParameters.cpp` | `src/graphics/SceneRenderer.cpp`, `tests/GameTests.cpp`, `tests/EditorTests.cpp` |
| Aparência da sala | `src/graphics/RoomBackdropRenderer.cpp` | `src/game/Room.hpp` |
| Camadas e composição | `src/graphics/SceneRenderer.cpp` | âncoras em `SceneObject` e `SpriteClip` |
| Comandos DEBUG e área do painel | `src/debug/DebugEditor.*` (opcional, `POINTCLICK_ENABLE_TOOLS`) | `src/debug/DebugOverlay.cpp`, `tests/EditorTests.cpp`; o que o editor pede ao app (recarregar PNGs) entra por callback ligado em `Application` (`setArtReloader`) |
| Desenho de diagnóstico (rota, painel técnico, nova visão de debug) | `DebugViews` + `kViewSwitches` em `src/debug/DebugEditor.*` (comando `show <visão> <0\|1>`) | desenho em `src/debug/DebugOverlay.cpp` / `src/debug/DiagnosticsOverlay.*`; `tests/EditorTests.cpp`; nunca em `graphics/` (`testGameFrameHasNoDebugDrawing` em `tests/SpriteTests.cpp`) |
| Carregar/salvar salas, layout dos assets | `src/content/RoomLibrary.*` | `core/Application.cpp` e `DebugEditor` usam a mesma biblioteca |
| Dependência entre camadas, targets CMake | `CMakeLists.txt` (um target por pasta) | `cmake/CheckLayerIncludes.cmake`, `docs/ARCHITECTURE.md` |
| Entrada multiplataforma e roteamento UI | `src/input/InputSystem.*`, `src/input/InputRouter.*` | `src/core/Application.cpp`, `src/graphics/Renderer.cpp`, `tests/EditorTests.cpp` |
| Regras de interação, itens e puzzles | `assets/rooms/*.rules`, `src/game/Interaction.*`, `src/content/InteractionFiles.*` | `src/game/Game.cpp`, `tests/GameTests.cpp`, `tests/EditorTests.cpp` |
| Efeito ou condição nova em `.rules` | uma entrada + uma função (`start`, e `update` se bloquear) em `src/game/ActionCatalog.cpp` | método em `src/game/ScriptHost.hpp` + `Game::Host` só se precisar de capacidade nova da cena; tabela em `docs/GAME_DESIGN.md`; `tests/EditorTests.cpp` (round-trip do catálogo) |
| Sequências, cutscene, pilha de sequências | `src/game/ScriptRunner.*`, `src/game/ScriptHost.hpp` | `Game::Host`, `Game::inCutscene` em `src/game/Game.cpp`, `tests/GameTests.cpp` |
| Diálogo com escolhas (formato `.dlg`) | `assets/rooms/*.dlg`, `src/content/DialogueFiles.*`, `src/game/Dialogue.*` | `start_dialogue` em `src/game/ActionCatalog.cpp`, `Game::chooseDialogueOption`, `tests/GameTests.cpp`, `tests/EditorTests.cpp` (round-trip), `docs/GAME_DESIGN.md` |
| Menu de escolhas na tela | `src/ui/DialogueMenu.*` (layout + `ViewportUiConsumer`) | `src/graphics/DialogueMenuRenderer.cpp`, registro em `src/core/Application.cpp`, `tests/EditorTests.cpp` |
| Tokenizer/leitura/gravação dos formatos de texto | `src/content/TextFormat.*` | `InteractionFiles`, `DialogueFiles` |
| Progresso de partida (flags, inventário, overrides) | `src/game/SessionState.*` | `src/game/Game.cpp`, `src/graphics/SceneRenderer.cpp`, `tests/GameTests.cpp`; campo novo também em `SessionSnapshot` e `src/content/SaveFiles.cpp` |
| Save/load de partida (formato `save.slot`, autosave; chave nova = `currentVersion` + 1) | `src/content/SaveFiles.*`, `src/game/SavedGame.hpp`, `src/game/Autosave.*` | `SessionState::snapshot/restore`, `Application::resumeSavedGame`/`processGameEvents`/`lifecycleWatch`, `tests/GameTests.cpp`, `docs/ARCHITECTURE.md` |
| UI de Inventário futuro | novo módulo em `src/ui/` consumindo `SessionState` / `Game::useItemOnObject` | modelo em `src/ui/DialogueMenu.*`, `ViewportUiConsumer` em `src/input/InputRouter.hpp`, desenho em `graphics/` |
| Efeitos visuais de transição de sala | `RoomChangeRequested` / `RoomEntered` em `Application::processGameEvents` | efeito visual temporizado em `graphics/` |
| Notificação nova para fora do domínio (áudio, autosave, legendas) | struct nova em `src/game/GameEvent.hpp` + `emit` em `src/game/Game.cpp` | consumidor em `Application::processGameEvents`, `testGameEventQueue` em `tests/GameTests.cpp` |
| Estado ou regra local da sala (aproximação, picking, fala na tela) | `src/game/Scene.*` | `Game` só coordena; `src/graphics/SceneRenderer.cpp` lê `const Scene&` |

## Contratos

- `Room`, `SceneObject`, `SpriteClip`, `AnimationSet`, `RoomInteractions` e
  `RoomDialogues` são definições autoradas/editáveis. Não guarde neles progresso de diálogo, itens
  coletados, flags ou tempo corrente.
- `SessionState` guarda o progresso persistente da sessão (flags/contadores,
  itens de inventário, overrides por `roomId + objectId` de visibilidade e
  colisão `solidOverride`, e as escolhas `once` de diálogo já feitas). É o que vai para o
  save (`SessionSnapshot`), junto com a sala atual e os pés; nada além disso. Editar a sala em tempo real no `DebugEditor`
  (`Game::applyRoom(room, false, error)`) ou trocar de sala **nunca** apaga o
  `SessionState`.
- `World::width` × `World::height` (`1280 × 720`) definem o campo de visão fixo da
  câmera/viewport lógica; o tamanho total da cena pertence a `Room::width` e
  `Room::height`. UI e overlays usam coordenadas de viewport; o mundo usa coordenadas
  da sala deslocadas por `Camera`.
- `Scene` (com `Player`, `Camera`, `Navigation`, `PendingInteraction`, `AnimationPlayback`),
  `ScriptRunner` e `DialoguePlayer` guardam estado de execução da cena. `Game` só coordena
  essas peças e `SessionState`; comunica para fora por `GameEvent`, nunca por campo + `consumeX()`. Regras de gameplay não devem carregar texturas nem
  fazer chamadas de desenho.
- `input/` traduz eventos brutos de mouse/teclado, toque (ignorando mouse sintético)
  e gamepad (hotplug, eixo analógico com *deadzone* e cursor virtual) para a viewport
  lógica e ações abstratas (`ToolAction`, `UiNavAction`), roteando por prioridade
  (`Ciclo de Vida → ToolInputConsumer opcional → ViewportUiConsumer → Game`) sem depender
  de `graphics/`, `ui/` ou `debug/`. A ferramenta de debug é opcional no build
  (`POINTCLICK_ENABLE_TOOLS`) e entra como consumidor e como `RenderOverlay`.
- `graphics/` lê o estado do jogo (`const Scene&` para a cena, `const Game&` só para o
  estado de diálogo e ferramentas, respeitando `Scene::isObjectVisible` e
  `Scene::isObjectSolid`) e desenha. Não decide resultados de interação,
  hit-test de UI, colisão, diálogo ou transição de sala.
- `content/` traduz arquivos `.room` (estética/espaço) e arquivos companheiros
  opcionais `.rules` (regras declarativas de interação) e `.dlg` (diálogos) em
  definições validadas por linha e de volta para arquivos, com o tokenizer comum
  de `content/TextFormat`. Quem carrega a sala confere as referências cruzadas
  (`dialogueReferencesError`) antes de aplicar. Preserve a leitura de chaves antigas ao evoluir.
- `ui/` contém UIs de gameplay (`ViewportUiConsumer`): definem a própria geometria na
  viewport, decidem o hit-test e chamam comandos do domínio (`Game::chooseDialogueOption`).
  Só agem no momento em que o domínio pede (`Game::awaitingDialogueChoice`, parte do
  `inCutscene`). `graphics/` lê essa geometria para desenhar.
- `debug/` altera a definição da sala para teste e define sua própria geometria de
  captura na viewport (`DebugEditor::panelRect`), preservando `SessionState`.
  Não é o sistema de gameplay. Todo desenho de diagnóstico (rota, destino, painel técnico,
  guias) mora aqui, atrás do editor e de `DebugViews`: `graphics/` desenha só o jogo, e uma
  build sem ferramentas não tem nenhum desses desenhos.
- IDs de objetos, itens, flags e nomes de animação são estáveis, com letras,
  números, `_` e `-`. Use esses nomes nas regras, sem depender da posição no vetor da sala.

## Verificar mudanças

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Há também uma build local em `cmake-build-debug`, caso já esteja configurada.
Para testar arquivos da raiz do projeto, execute o jogo com `--rooms assets/rooms`.
