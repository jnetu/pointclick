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
| Aparência da sala | `src/graphics/RoomBackdropRenderer.cpp` | `src/game/Room.hpp` |
| Camadas e composição | `src/graphics/SceneRenderer.cpp` | âncoras em `SceneObject` e `SpriteClip` |
| Comandos DEBUG | `src/debug/DebugEditor.cpp` | `tests/EditorTests.cpp` |
| Entrada da janela e viewport | `src/core/Application.cpp`, `src/graphics/Renderer.cpp` | `tests/EditorTests.cpp` |
| Diálogo futuro | novo módulo em `src/game/` para estado e regras | roteamento em `Application`, interface em `graphics/` |
| Transição de sala futura | definição em `content/`, troca de estado em `Game` | efeito visual em `graphics/` |

## Contratos

- `Room`, `SceneObject`, `SpriteClip` e `AnimationSet` são definições editáveis.
  Não guarde neles progresso de diálogo, estado de interação ou tempo corrente.
- `Game`, `Player` e `AnimationPlayback` guardam estado de execução. Regras de
  gameplay não devem carregar texturas nem fazer chamadas de desenho.
- `graphics/` lê o estado do jogo e desenha. Não decide resultados de interação,
  colisão, diálogo ou transição de sala.
- `content/` traduz arquivos `.room` em definições e de volta para arquivos.
  Preserve a leitura de chaves antigas ao evoluir o formato.
- `debug/` altera a definição da sala para teste. Não é o sistema de gameplay.
- IDs de objetos e nomes de animação são estáveis, com letras, números, `_` e
  `-`. Use esses nomes nas regras, sem depender da posição no vetor da sala.

## Verificar mudanças

```sh
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

Há também uma build local em `cmake-build-debug`, caso já esteja configurada.
Para testar arquivos da raiz do projeto, execute o jogo com `--rooms assets/rooms`.
