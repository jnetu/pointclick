# Organização e extensão

A configuração da sala é um conjunto de dados. O estado do jogo, a entrada,
a renderização e as ferramentas de edição têm responsabilidades separadas.

| Local | Responsabilidade |
| --- | --- |
| `assets/rooms/*.room` | Salas editáveis por game design (geometria, estética, colisão base e sprites) |
| `assets/rooms/*.rules` | Regras declarativas opcionais de interação, coleta e puzzles por sala |
| `assets/sprites/*.png` | Imagens estáticas e folhas de animação |
| `game/RoomParameters` | Catálogo de parâmetros: nome, descrição, intervalo, passo e acesso ao campo |
| `content/RoomFiles` | Leitura e gravação das salas; erros com número de linha |
| `content/InteractionFiles` | Parser, validação por linha e gravação atômica dos arquivos companheiros `.rules` |
| `content/SpriteFields` | Leitura e gravação dos campos de um clip, independente da sala |
| `content/RoomPresets` | Sprites, sala e regras padrão embutidos usados por testes e instanciação direta |
| `game/Room` | Dados da sala, validação, escala, velocidade e limites |
| `game/SceneObject` | Identidade, desenho, tipo e base local de colisão |
| `game/SpriteClip` | Definição de arquivo, corte, tempo e âncora visual de um sprite |
| `game/Animation` | Clips nomeados e reprodução por entidade |
| `game/ObjectAnimations` | Estado de reprodução dos objetos, por ID, preservado nas edições |
| `game/SessionState` | Progresso persistente da partida: flags/contadores, inventário e overrides por sala/objeto |
| `game/Interaction` | Gatilhos, condições, efeitos, picking por profundidade e cálculo de aproximação |
| `game/Camera` | Câmera 2D suave com campo de visão fixo (`1280×720`), clamp nas bordas da sala e interpolação |
| `game/Game` | Coordena entrada de gameplay, Walk-to-Interact, estado de sessão, salas, navegação, câmera e jogador |
| `game/Player` | Posição atual/anterior e execução dos pontos da rota |
| `game/Navigation` | Grade dinâmica por sala, obstáculos, A* e simplificação segura das rotas |
| `debug/DebugEditor` | Reconhece DEBUG/F1/Select, decide hit-test do painel na viewport, processa comandos e gerencia edição/undo |
| `input/InputSystem` | Traduz mouse/teclado, toque (sem duplicação sintética), gamepad (hotplug, `moveAxis` com *deadzone* e cursor virtual) e ciclo de vida |
| `input/InputRouter` | Cadeia de consumo por prioridade: Ciclo de Vida → `DebugEditor` → `ViewportUiConsumer` (UIs futuras) → `Game` |
| `graphics/RoomBackdropRenderer` | Parede, chão e linhas de perspectiva deslocados pela câmera |
| `graphics/EntityRenderer` | Desenho do player e dos objetos deslocados pela câmera |
| `graphics/SceneRenderer` | Ordenação por profundidade e composição da cena (filtrando objetos ocultos em `SessionState`) |
| `graphics/SpriteLibrary` | Carregamento de PNG, cache de texturas, corte e imagem quadriculada de substituição |
| `graphics/DiagnosticsOverlay` | Textos técnicos de posição, sala, câmera e viewport fixa |
| `graphics/DebugOverlay` | Guias de objetos no mundo e desenho do painel consultando `DebugEditor::panelRect()` |
| `graphics/Renderer` | Ciclo de desenho, apresentação lógica `1280×720` com letterbox, cursor virtual do controle e conversão janela/toque/viewport |
| `core/Application` | Janela, ciclo de vida mobile/console, carregamento `.room` + `.rules`, transições de sala e loop em passo fixo |

O executável e os testes compartilham `pointclick_core`. As rotinas de desenho
ficam em `pointclick_graphics`. Uma correção no jogo ou na entrada chega aos dois
consumidores. As dependências seguem `core` → `graphics`/`input`/`debug`/`content` → `game`.
`input` não depende de `graphics` (recebe `ViewportCoordinateMapper` injetado);
`graphics` e `content` leem tipos de `game`; o domínio do jogo não carrega
arquivos de arte nem chama funções de desenho.

## Fluxos principais

`SDL_Event` (mouse/teclado, toque ou gamepad) → `InputSystem::translateEvent` (com
`Renderer::viewportMapper` para mapear janela e toque `[0..1]` à viewport lógica
`1280×720` respeitando letterbox/pillarbox, filtrando `SDL_TOUCH_MOUSEID` /
`SDL_MOUSE_TOUCHID` e atualizando o cursor virtual pelo `moveAxis` do analógico) →
`InputRouter` (Consumer Chain em 4 camadas):
1. **Ciclo de Vida e Sistema**: `SDL_EVENT_QUIT`, suspensão mobile/console
   (`SDL_EVENT_WILL_ENTER_BACKGROUND` / `SDL_EVENT_DID_ENTER_FOREGROUND`, pausando
   renderização em background e limpando eixos transitórios) e reset gráfico
   (`SDL_EVENT_RENDER_DEVICE_RESET` / `SDL_EVENT_RENDER_TARGETS_RESET` / `SDL_EVENT_LOW_MEMORY`,
   invalidando texturas em `Renderer`).
2. **Ferramentas (`DebugEditor`)**: ativado por `DEBUG`, `F1` ou botão `BACK`/`Select`
   do controle; consome comandos (`DebugKey` via setas, D-Pad ou ombros), texto e
   cliques dentro de `DebugEditor::panelRect()` quando visível (`consumesViewportPoint`).
3. **UIs de Gameplay (`ViewportUiConsumer`)**: ponto de extensão na viewport fixa
   `0..1280 × 0..720` para futuros Diálogos, Inventário e Menus antes da câmera.
4. **Mundo (`Game`)**: converte `viewportPoint` via `Camera::viewportToWorld` e
   entrega `Game::onPointerMove` / `Game::onClick`:
   - `pickInteractiveObject` verifica se o clique atingiu algum `SceneObject`
     visível (`SessionState::isObjectVisible`) que possua regras em `RoomInteractions`,
     escolhendo o objeto mais à frente (`depth()`, ordem exatamente inversa ao
     `std::stable_sort` de `SceneRenderer`).
   - Se atingiu um objeto interativo, registra `PendingInteraction` e calcula a rota
     até o ponto alcançável mais próximo da base (`Navigation::findPathToNearestReachable`).
     Se o jogador já estiver parado nesse ponto, a interação resolve imediatamente;
     caso contrário, resolve assim que `Player` conclui a caminhada no `tick()`.
   - Se clicou no chão vazio, cancela qualquer `PendingInteraction` em andamento e
     caminha até o novo destino.

A cada tick, o jogo calcula a velocidade na profundidade atual; o player avança
sem ultrapassar o próximo ponto, resolve interações pendentes ao chegar e a `Camera`
atualiza sua posição suavizada dentro dos limites `[0, room.width - 1280] × [0, room.height - 720]`
(permanecendo fixa em `(0, 0)` nas salas `1280×720`). Ao carregar/resetar uma sala ou
teleportar o player, a câmera faz *snap* imediato ao alvo; em edições ao vivo do editor,
preserva a continuidade respeitando os novos limites. O render usa posições interpoladas
tanto para o `Player` quanto para a `Camera`.

Texto → `InputRouter` → `DebugEditor::onTextInput` → ativação por DEBUG. Em
plataformas com teclado virtual na tela (`SDL_HasScreenKeyboardSupport`), o texto
só é ativado sob demanda quando o painel do editor está visível sem uso de gamepad,
evitando abrir o teclado virtual no boot mobile/console. Enquanto ativo, o editor
consome texto e teclas de edição. As alterações passam por `Game::applyRoom`;
ele valida os dados e prepara uma nova navegação (já considerando os overrides de
visibilidade e `solidOverride` do `SessionState`) antes de substituir o estado.
Uma edição rejeitada não altera sala, regras, posição ou rota. Uma edição aceita
interrompe a rota e preserva os pés e todo o `SessionState`.

`save` grava primeiro um arquivo temporário e depois o substitui no destino.
`load` passa pela mesma validação usada pelas edições e carrega também o arquivo
companheiro opcional `<sala>.rules` via `InteractionFiles::loadOptionalForRoom`.
A aplicação carrega `first.room` (e `first.rules`, se existir) da pasta indicada
por `--rooms` na inicialização, e processa `Game::consumeRoomTransition()` para
trocar de sala automaticamente quando um efeito `change_room <roomId>` dispara.

A leitura de `.room` e `.rules` usa `SDL_IOFromFile` e `SDL_LoadFile_IO` do SDL3
3.4.16, com caminhos UTF-8 e o tamanho explícito dos dados. O stream permanece aberto
para conferir erros de leitura antes do parser; stream e buffer são liberados por RAII.
Sem `--rooms`, o caminho continua sendo `SDL_GetBasePath()/assets/rooms`.
No Android, a base é `./` e o SDL remove esse prefixo ao procurar no pacote.
O empacotamento deve preservar `assets/rooms` dentro do espaço de assets do APK
(por exemplo, `app/src/main/assets/assets/rooms/first.room`), ou usar `--rooms`
para indicar outra raiz. `SDL_GetBasePath` devolve um ponteiro mantido pelo SDL,
que o chamador não deve liberar.

A gravação continua usando `std::ofstream` e o sistema de arquivos local.
Essa migração de leitura não define um diretório gravável para saves em mobile
nem implementa suporte mobile completo.

## Adicionar um parâmetro numérico

1. Adicione o campo e seu padrão em `Room.hpp`.
2. Registre nome, descrição, intervalo e passo em `game/RoomParameters.cpp`.
3. Use o campo na mecânica correspondente. Se depender de outro campo,
   adicione a verificação em `Room::validationError()`.
4. Documente o significado e acrescente um teste do comportamento relevante.

O editor, `set`, a leitura e a gravação passam a conhecer esse parâmetro pelo
catálogo. A lista do editor muda de página automaticamente conforme a seleção.
Novos tipos de dados precisam de uma representação explícita em `RoomFiles`.

## Separação Tripla de Estado e Interações Declarativas

O projeto separa estritamente três categorias de estado:
1. **Definições Autoradas (`Room`, `SceneObject`, `SpriteClip`, `RoomInteractions`)**:
   Imutáveis durante o gameplay; carregadas de `.room` e `.rules` ou de presets C++.
2. **Estado de Simulação Local (`Player`, `Camera`, `Navigation`, `ObjectAnimations`, `PendingInteraction`)**:
   Estado volátil da sala atual, atualizado em `Game::tick(dt)` e lido em `graphics/`.
3. **Estado de Sessão (`SessionState`)**:
   Armazena flags/contadores (`setFlag`, `clearFlag`, `hasFlag`), itens ordenados do
   inventário (`addItem`, `removeItem`, `hasItem`, `items`) e overrides de objetos por
   `roomId + objectId` (`setObjectVisible` / `isObjectVisible` e `setObjectSolid` /
   `isObjectSolid`). Persiste intacto através de trocas de sala (`loadRoom`) e de
   edições em tempo real no `DebugEditor` (`applyRoom(room, false, error)`).

Em `Interaction` / `InteractionFiles`, cada `InteractionRule` possui:
- **Gatilho (`InteractionTrigger`)**: `interact <objectId>` (clique direto) ou
  `use_item <itemId> <objectId>` (acionável via `Game::useItemOnObject` / `Game::useItemAt`,
  pronto para a UI de inventário).
- **Condições (`InteractionCondition`)**: `flag_set`, `flag_not_set`, `has_item`,
  `lacks_item`, `object_visible`, `object_hidden`.
- **Efeitos (`InteractionEffect`)**: `say "<texto>"`, `set_flag`, `clear_flag`,
  `give_item`, `take_item`, `hide_object`, `show_object`, `set_solid <objectId> <0|1>`
  (reconstrói `Navigation` automaticamente), `play_animation <objectId> <anim>` e
  `change_room <roomId>`.

As regras são avaliadas na ordem declarada: a primeira regra cujas condições forem
todas verdadeiras executa seus efeitos em sequência e encerra a avaliação. Objetos
ocultos (`!isObjectVisible`) deixam imediatamente de ser desenhados em `SceneRenderer`,
deixam de bloquear `Navigation` e são ignorados no clique (`pickInteractiveObject`).

Para adicionar arte, configure um `SpriteClip` no conjunto de animações da sala
ou do objeto. A definição guarda o caminho relativo e o corte da imagem, sem carregar texturas. O
`SpriteLibrary` carrega PNGs sob `assets/sprites`, mantém as texturas em cache e
desenha o quadriculado de substituição quando o PNG ou corte falha. Mudanças em
`SessionState` (coletar itens, abrir portas, ativar flags) não incrementam
`roomRevision()`, preservando o cache de texturas da GPU.

## Evoluir diálogos, inventário visual e transições

Os cinco nomes de locomoção do jogador (`idle`, `left`, `right`, `up`, `down`)
são escolhidos em `Player.cpp`. Para animações de objetos acionadas por puzzles,
use o efeito declarativo `play_animation <objeto> <clip>` no `.rules` ou
`Game::playObjectAnimation` em C++; a conclusão de um clip sem repetição pode ser
consultada com `AnimationPlayback::finished`. Não use um novo valor de
`SceneObjectType` para cada animação: esse enum expressa apenas se a base autoral
do objeto bloqueia o caminho por padrão.

Para uma futura UI visual de inventário ou árvore de diálogos com múltiplas escolhas,
implemente um `ViewportUiConsumer` registrado em `InputRouter` operando na viewport
fixa `1280×720`. A UI de inventário pode ler `game.session().items()` e disparar
`game.useItemAt(itemId, worldPoint)` ou `game.useItemOnObject(itemId, objectId)`
sem acoplar `graphics/` às regras de negócio. Efeitos visuais de transição de sala
(*fade*) podem observar a troca em `Application` / `Renderer` usando estado
temporizado próprio no renderizador, sem alterar colisões ou conteúdo da sala.

## Escopo atual

A viewport lógica da câmera é estritamente fixa em `1280 × 720` (`World::width × World::height`)
com `SDL_LOGICAL_PRESENTATION_LETTERBOX`: monitores 2K, 4K, Ultrawide, celulares e
consoles exibem exatamente o mesmo enquadramento do mundo, sem revelar áreas extras.
Cada sala define suas próprias dimensões (`room.width` e `room.height`, mínimo
`1280 × 720`); salas maiores rolam com `Camera` 2D suave e redimensionam a grade
retangular de `Navigation`. Formatos de chão com polígonos, múltiplos andares e
obstáculos móveis contínuos exigirão ampliar esse modelo. O formato `.room` permanece
puramente espacial/estético, enquanto a lógica condicional declarativa fica isolada
nos arquivos companheiros opcionais `.rules`.

O player é um ponto nos pés para a navegação, com margens configuráveis em volta
dos obstáculos. A largura visual varia com a perspectiva; ajuste as margens
quando mudar essa largura. Não há colisão física por pixel de sprite.

## Verificação

```sh
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

Os testes cobrem movimento, perspectiva, desvio, quinas, salas panorâmicas,
câmera fixa e suave com clamp nas bordas, Walk-to-Interact (aproximação, cancelamento
no meio do caminho, disparo imediato quando já posicionado e picking por profundidade),
coleta de itens, puzzles/NPCs condicionais com desbloqueio dinâmico de `Navigation`,
persistência de `SessionState` entre salas e durante edições do `DebugEditor` (`set`,
`move`, `undo`, `reset`), parser/serialização/validação por linha de `.rules`,
configurações inválidas, entrada multiplataforma e independência de resolução (720p,
1080p, 2K, 4K e Ultrawide). Os testes de sprites cobrem cortes, animação, seleção de
direção, persistência e imagem de substituição. Os testes gráficos usam o driver
virtual e o renderizador de software do SDL. As verificações também ficam ativas
em Release.
