# Organização e extensão

A configuração da sala é um conjunto de dados. O estado do jogo, a entrada,
a renderização e as ferramentas de edição têm responsabilidades separadas.

| Local | Responsabilidade |
| --- | --- |
| `assets/rooms/*.room` | Salas editáveis por game design (geometria, estética, colisão base e sprites) |
| `assets/rooms/*.rules` | Regras declarativas opcionais de interação, coleta e puzzles por sala |
| `assets/rooms/*.dlg` | Diálogos opcionais da sala: nós, falas, escolhas condicionais e `goto` |
| `assets/sprites/*.png` | Imagens estáticas e folhas de animação |
| `game/RoomParameters` | Catálogo de parâmetros: nome, descrição, intervalo, passo e acesso ao campo |
| `content/RoomFiles` | Leitura e gravação das salas; erros com número de linha |
| `content/InteractionFiles` | Parser, validação por linha e gravação atômica dos arquivos companheiros `.rules` |
| `content/DialogueFiles` | Parser, validação por linha e gravação atômica dos arquivos companheiros `.dlg` |
| `content/TextFormat` | Tokenizer comum (comentários, aspas, instruções do catálogo), leitura SDL, gravação atômica e o texto dos números (`number`: o decimal mais curto que relê o mesmo `float`, sem depender de locale) |
| `content/SpriteFields` | Leitura e gravação dos campos de um clip, independente da sala |
| `tests/support/RoomPresets` | Sala, sprites e regras de exemplo em C++ **só para testes** (target `pointclick_test_support`; nem o jogo nem o leitor de salas os linkam) |
| `content/ActorFiles` | Leitura de `assets/actors/<id>.actor`: clips de um ator compartilhados por todas as salas |
| `content/RoomLibrary` | Layout dos assets (`rooms/`, `actors/`, `sprites/`) e carga de sala + `.rules` + `.dlg` validados juntos; usado pelo jogo e pelo editor |
| `content/ContentCheck` | `pointclick --check`: lista a pasta (só desktop), carrega cada sala por `RoomLibrary::loadBundle` + `transitionsError` e devolve os erros por arquivo e linha |
| `core/CommandLine` | Opções `--rooms`, `--new-game`, `--no-save`, `--room <sala> [spawn]`, `--check`, lidas sem SDL (`pointclick_cli`, testável) |
| `game/Room` | Dados da sala, validação, escala, velocidade e limites |
| `game/SceneObject` | Identidade, desenho, tipo e base local de colisão |
| `game/SpriteClip` | Definição de arquivo, corte, tempo e âncora visual de um sprite |
| `game/Animation` | Clips nomeados e reprodução por entidade |
| `game/ObjectAnimations` | Estado de reprodução dos objetos, por ID, preservado nas edições |
| `game/SessionState` | Progresso persistente da partida: flags/contadores, inventário e overrides por sala/objeto; `snapshot()`/`restore()` por valor (`SessionSnapshot`) para o save |
| `game/SavedGame` | O que o save guarda: sala atual, pés e `SessionSnapshot` (tipo de valor, sem I/O) |
| `game/Autosave` | `AutosavePolicy`: último snapshot estável (fora de cutscene) e quando gravar (uma vez por ida ao background, ao sair) |
| `content/SaveFiles` | Formato `save.slot`, leitura com erro por linha, gravação atômica, caminho no `SDL_GetPrefPath`, quarentena `.bad` e retomada (`resume`) |
| `game/ScriptRunner`, `game/ScriptHost` | Executor de sequências empilháveis de ações por tick, modo cutscene e contrato da cena para as ações |
| `game/Dialogue` | Definição dos diálogos (`RoomDialogues` → `DialogueNode` → `DialogueChoice`) e `DialoguePlayer`, a conversa em andamento sobre o `ScriptRunner` |
| `game/ActionCatalog` | Catálogo tipado de condições e efeitos das regras (`ArgType`, validação, execução, eventos de saída) |
| `game/Interaction` | Gatilhos, condições, efeitos, picking por profundidade e cálculo de aproximação |
| `game/Camera` | Câmera 2D suave com campo de visão fixo (`1280×720`), clamp nas bordas da sala e interpolação |
| `game/Game` | Coordenador fino: recebe intenções (clique, uso de item, escolha de diálogo), dono de `SessionState`, `Scene`, `ScriptRunner`, `DialoguePlayer` e da fila de `GameEvent` |
| `game/Scene` | Sala em jogo: conteúdo carregado (sala, regras, diálogos) e simulação local (navegação, jogador, câmera, animações, fala, Walk-to-Interact, picking) |
| `game/GameEvent` | Notificações para fora do domínio (`RoomEntered`, `RoomChangeRequested`, `LineSpoken`, `CutsceneStarted`/`Ended`) |
| `game/Player` | Posição atual/anterior e execução dos pontos da rota |
| `game/Navigation` | Grade dinâmica por sala, obstáculos, A* e simplificação segura das rotas; destino inalcançável = um flood-fill + um A* |
| `game/StringMap` | Mapa por `std::string` com hash transparente (consulta por `string_view` sem alocar) |
| `debug/DebugEditor` | Ferramenta opcional (`POINTCLICK_ENABLE_TOOLS`): `ToolInputConsumer` que reconhece DEBUG/F1/Select, faz o hit-test do painel, processa comandos e gerencia edição/undo |
| `debug/DebugOverlay` | `RenderOverlay` da ferramenta: com o editor ativo, guias de objetos, rota e cruz de destino, painel técnico e painel do editor (`DebugEditor::panelRect()`); fechado, só a dica de como abrir. Cada visão liga/desliga por `DebugViews` (`show <visão> <0\|1>`) |
| `debug/DiagnosticsOverlay` | Textos técnicos de posição, sala, câmera e viewport fixa, desenhados pelo `DebugOverlay` |
| `input/InputSystem` | Traduz mouse/teclado, toque (sem duplicação sintética), gamepad (hotplug, `moveAxis` com *deadzone* e cursor virtual) e ciclo de vida |
| `input/InputRouter` | Cadeia de consumo por prioridade: Ciclo de Vida → `ToolInputConsumer` opcional → `ViewportUiConsumer` (UIs de gameplay) → `Game` |
| `ui/DialogueMenu` | Menu de escolhas: layout na viewport (linhas medidas e quebradas com `TextMetrics`, em cache), hit-test, destaque por ponteiro/setas e `Game::chooseDialogueOption` |
| `ui/TextMetrics` | `FontRole` (papéis semânticos do texto) e a interface de medida em unidades da viewport, injetada nos layouts de `ui/`; `MonospaceTextMetrics` para testes e fallback |
| `ui/TextLayout` | Quebra por palavras UTF-8 (e por `\n`) com medida injetável, sem cópia (`TextLine` = faixa de bytes), e `placeAbove` (caixa centrada e mantida dentro de uma área) |
| `ui/SpeechLayout` | Layout da fala: quebra até 820 px, nome do orador opcional, centralizada acima da cabeça e dentro da viewport; refaz a quebra só quando texto ou métrica mudam |
| `graphics/DialogueMenuRenderer` | Desenho do menu de escolhas com o layout de `DialogueMenu` |
| `graphics/FontLibrary` | Único mapeamento `FontRole` → arquivo TTF/tamanho/contorno; abre por `SDL_IOStream` (funciona no APK/bundle) na escala de pixels da saída |
| `graphics/TextRenderer` | SDL3_ttf: implementa `TextMetrics`, cache de `TTF_Text` por (papel, linha) com descarte por desuso, atlas de glifos do *text engine*, contorno escuro, desenho 1:1 em pixels de saída (DPI), fallback para a fonte de debug por papel |
| `graphics/SpeechRenderer` | Desenha `Scene::floatingText` com `SpeechLayout`, ancorada em `EntityRenderer::playerRect` |
| `graphics/RoomBackdropRenderer` | Parede, chão e linhas de perspectiva deslocados pela câmera |
| `graphics/EntityRenderer` | Desenho do player e dos objetos deslocados pela câmera |
| `graphics/SceneRenderer` | Ordenação por profundidade e composição da cena (filtrando objetos ocultos em `SessionState`) |
| `graphics/SpriteLibrary` | Carregamento de PNG, cache de texturas por arquivo (mantido nas edições; descarta o que ficou duas trocas de sala sem uso), corte e quadriculado de substituição |
| `graphics/RenderOverlay` | Ponto de extensão para camadas desenhadas por cima (a ferramenta de debug se registra aqui) |
| `graphics/Renderer` | Ciclo de desenho, apresentação lógica `1280×720` com letterbox, cursor virtual do controle e conversão janela/toque/viewport |
| `core/Application` | Janela, ciclo de vida mobile/console, retomada do save ou `first.room`, autosave, carregamento `.room` + `.rules` + `.dlg`, transições de sala e loop em passo fixo |

Cada camada é uma biblioteca estática, e o linker recusa dependências proibidas:

| Target | Pasta | Depende de |
| --- | --- | --- |
| `pointclick_game` | `src/game/` | SDL (só tipos de valor e helpers de `SDL_rect.h`) |
| `pointclick_content` | `src/content/` | `game` |
| `pointclick_input` | `src/input/` | `game` |
| `pointclick_ui` | `src/ui/` | `input`, `game` |
| `pointclick_graphics` | `src/graphics/` | `ui`, `input`, `game`, SDL3_ttf (privado) |
| `pointclick_tools` (opcional) | `src/debug/` | `content`, `graphics` |
| `pointclick_cli` | `src/core/CommandLine.cpp` | `game` (só `Identifier.hpp`) |
| `pointclick` | `src/core/`, `main.cpp` | todos; `pointclick_tools` só com `POINTCLICK_ENABLE_TOOLS=ON` |

`POINTCLICK_ENABLE_TOOLS` (padrão `ON`) liga o editor; com `OFF`, como numa build de
loja, o executável não contém `DebugEditor` e os testes do editor não são compilados.
`input/` emite ações abstratas (`ToolAction`, `UiNavAction`) e não conhece o editor: a
ferramenta se registra como `ToolInputConsumer` no `InputRouter` e como `RenderOverlay` no
`Renderer`. `pointclick_tests` linka só `content` + `game`, provando que o jogo roda sem
renderização. O teste `pointclick_layer_includes` (`cmake/CheckLayerIncludes.cmake`) falha
se uma pasta incluir uma camada proibida, se `game/` incluir SDL além de `SDL_rect.h` e
`SDL_pixels.h`, ou se uma definição autorada de `game/` (`Room`, `SceneObject`, `SpriteClip`,
`RoomParameters`, `PlayerClips`, `Facing`) incluir simulação ou sessão (`Player`, `Scene`,
`Game`, `Navigation`, `Camera`, `ScriptRunner`, `SessionState`). Os nomes dos clips do player
ficam em `game/PlayerClips.hpp` justamente para `Room` validar a sala sem conhecer `Player`.

Todos os targets compilam com `POINTCLICK_WARNINGS` (`-Wall -Wextra -Wpedantic -Wconversion
-Wshadow`; `/W4` no MSVC). No GCC entra também `-Wno-missing-field-initializers`: o `-Wextra`
dele acusa todo inicializador designado que omite um campo com valor padrão.

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
   do controle; consome ações (`ToolAction` via setas, D-Pad ou ombros), texto e
   cliques dentro de `DebugEditor::panelRect()` quando visível (`consumesViewportPoint`).
3. **UIs de Gameplay (`ViewportUiConsumer`)**: ponto de extensão na viewport fixa
   `0..1280 × 0..720` antes da câmera. `ui/DialogueMenu` é o primeiro: enquanto
   `Game::awaitingDialogueChoice()`, consome cliques/toques (clique numa linha escolhe;
   fora dela não faz nada), setas/D-pad e confirmação. Inventário e menus seguem o modelo.
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

Texto → `InputRouter` → `DebugEditor::onTextInput` → ativação por DEBUG →
`ViewportUiConsumer::onTextInput`. O mundo não recebe texto livre: o que ninguém
consome é descartado, então digitar nunca vira fala nem atrasa uma cutscene. Builds
sem ferramentas não iniciam entrada de texto. Em
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
por `--rooms` na inicialização. Depois de cada tick, `Application::processGameEvents`
esvazia a fila do `Game` e carrega a sala de um `RoomChangeRequested` (emitido quando
uma sequência com `change_room <roomId> [<spawn>]` termina), com os pés no `[spawn]` pedido
(`Room::entryPoint`; vazio = `playerStart`). `Scene::apply` resolve o spawn antes de mexer em
qualquer estado: spawn desconhecido recusa a carga inteira. Spawns de outras salas não são
conferidos a cada troca (exigiria ler as vizinhas); `RoomLibrary::transitionsError` faz essa
checagem cruzada como verificação de conteúdo (`pointclick --check`, testes; um futuro
manifesto no boot).

A leitura de `.room` e `.rules` usa `SDL_IOFromFile` e `SDL_LoadFile_IO` do SDL3
(versão de referência `POINTCLICK_SDL3_VERSION` em `cmake/Dependencies.cmake`), com caminhos UTF-8 e o tamanho explícito dos dados. O stream permanece aberto
para conferir erros de leitura antes do parser; stream e buffer são liberados por RAII.
Sem `--rooms`, o caminho continua sendo `SDL_GetBasePath()/assets/rooms`.
No Android, a base é `./` e o SDL remove esse prefixo ao procurar no pacote.
O empacotamento deve preservar `assets/rooms` dentro do espaço de assets do APK
(por exemplo, `app/src/main/assets/assets/rooms/first.room`), ou usar `--rooms`
para indicar outra raiz. `SDL_GetBasePath` devolve um ponteiro mantido pelo SDL,
que o chamador não deve liberar.

Texto de jogo: `RoomLibrary::fontsDirectory()` (`<assets>/fonts`, irmã de `rooms/` e
`sprites/`) é entregue a `Renderer::setFontRoot`, que abre as fontes pelo mesmo `SDL_IOStream`
(no Android, de dentro do APK). SDL3_ttf segue a regra do SDL3: `POINTCLICK_SDL3_TTF_VERSION`
em `cmake/Dependencies.cmake` é o mínimo do pacote do sistema e a versão fixa do CPM; sem pacote,
o CPM compila o SDL3_ttf com o FreeType do sistema ou, em mobile/console/MSVC (ou com
`POINTCLICK_VENDOR_FREETYPE=ON`), com o FreeType fixado (`POINTCLICK_FREETYPE_VERSION`).
HarfBuzz e plutosvg ficam desligados. Fonte ausente não impede o jogo: log e fonte de debug para
aquele papel. A escala de pixels (saída ÷ `1280×720`, em passos de 0,25) é conferida a cada quadro;
quando muda, as fontes são re-rasterizadas, o cache de linhas é limpo e `TextMetrics::revision()`
avança para os layouts de `ui/` refazerem a quebra. `RENDER_DEVICE_RESET` também recria o
*text engine*. O desenho de diagnóstico continua com `SDL_RenderDebugText`, só em `debug/`.

A gravação passa por `TextFormat::writeTextAtomic` (`std::ofstream` num `.tmp` e
`std::filesystem::rename`), sempre no sistema de arquivos local, com dois destinos: as salas
gravadas pelo editor vão para a pasta de `--rooms` (ferramenta de desenvolvimento, desktop) e o
save vai para `SDL_GetPrefPath`, a pasta gravável do usuário em toda plataforma (ver "Save/load
de partida"). Ainda não há projeto de empacotamento para Android/iOS nem teste em aparelho, e
consoles e Web vão pedir a troca dessa escrita pela API de armazenamento da plataforma
(`docs/ROADMAP.md`, L2 e L3). O `player.actor` e os companheiros opcionais (`.rules`, `.dlg`)
são encontrados abrindo o arquivo pelo SDL: `std::filesystem::exists` não enxerga assets dentro
do APK e só é consultado, depois de uma abertura que falhou, para separar "ilegível" de
"ausente".

## Adicionar um parâmetro numérico

1. Adicione o campo e seu padrão em `Room.hpp`.
2. Registre nome, descrição, intervalo e passo em `game/RoomParameters.cpp`.
3. Use o campo na mecânica correspondente. Se depender de outro campo,
   adicione a verificação em `Room::validationError()`.
4. Documente o significado e acrescente um teste do comportamento relevante.

O editor, `set`, a leitura e a gravação passam a conhecer esse parâmetro pelo
catálogo. A lista do editor muda de página automaticamente conforme a seleção.
Novos tipos de dados precisam de uma representação explícita em `RoomFiles`.

## Coordenador, cena e eventos

`Game` é um coordenador fino. Ele é dono de três peças e não guarda estado próprio além
da ligação entre elas:

- **`SessionState`**: progresso da partida, que sobrevive a trocas de sala e edições.
- **`Scene`**: a sala em jogo. Lê a sessão (visibilidade, colisão, inventário) por
  referência constante e nunca a altera; quem altera são os scripts e, por fora deles,
  `Game::changeSession`.
- **`ScriptRunner` + `DialoguePlayer`**: executam sequências e conversas sobre as duas,
  pelo adaptador `Game::Host` (`ScriptHost`).

Para dentro, o `Game` recebe intenções (`onClick`, `useItemOnObject`, `startDialogue`,
`chooseDialogueOption`, `tick`). Para fora, emite `GameEvent` (`std::variant`) numa fila
em ordem. `Application` esvazia a fila com `drainEvents(buffer)` (troca de vetores, o
buffer é reaproveitado) e encaminha cada evento. Sistemas novos (áudio, fade de transição,
autosave, legendas) assinam ali, sem o domínio conhecê-los. Não crie campo + `consumeX()`
na fachada; crie um tipo de evento em `game/GameEvent.hpp` e emita no `Game`.

| Evento | Quando |
| --- | --- |
| `RoomEntered{roomId, spawnId}` | uma carga com spawn foi aplicada (`spawnId` vazio = `playerStart`) (`loadRoom`/`applyRoom(..., true, ...)`); edições ao vivo não emitem |
| `RoomChangeRequested{roomId, spawnId}` | uma sequência com `change_room` terminou; é descartado se outra sala entrar antes de a fila ser esvaziada |
| `LineSpoken{text}` | um `say` começou |
| `CutsceneStarted` / `CutsceneEnded` | `inCutscene()` mudou |

`RoomEntered` e `CutsceneEnded` são os pontos estáveis do autosave (ver "Save/load de partida").

## Separação Tripla de Estado e Interações Declarativas

O projeto separa estritamente três categorias de estado:
1. **Definições Autoradas (`Room`, `SceneObject`, `SpriteClip`, `RoomInteractions`)**:
   Imutáveis durante o gameplay; carregadas de `.room` e `.rules` ou de presets C++.
2. **Estado de Simulação Local (`Player`, `Camera`, `Navigation`, `ObjectAnimations`, `PendingInteraction`)**:
   Estado volátil da sala atual, agrupado em `Scene`, avançado por `Game::tick(dt)` e lido
   em `graphics/` (`SceneRenderer` recebe só `const Scene&`).
3. **Estado de Sessão (`SessionState`)**:
   Armazena flags/contadores (`setFlag`, `clearFlag`, `hasFlag`), itens ordenados do
   inventário (`addItem`, `removeItem`, `hasItem`, `items`) e overrides de objetos por
   `roomId + objectId` (`setObjectVisible` / `isObjectVisible` e `setObjectSolid` /
   `isObjectSolid`). O `roomId` é o nome do arquivo `<id>.room`: `RoomLibrary` recusa
   um arquivo cujo `id` difere do nome e grava o `id` do nome ao salvar. Persiste intacto através de trocas de sala (`loadRoom`) e de
   edições em tempo real no `DebugEditor` (`applyRoom(room, false, error)`).
   `Game::session()` é só leitura. Fora de um script, quem muda o progresso (ferramenta, UI,
   teste) chama `Game::changeSession([](SessionState& s) { ... })`: o `Game` aplica a função,
   compara quais objetos da sala bloqueiam o chão antes e depois e, se algo mudou, reconstrói
   a navegação (tirando o player de dentro de um obstáculo novo). A rota já em curso ainda
   não é replanejada.

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

Condições e efeitos vêm do **catálogo de ações** (`game/ActionCatalog`), no mesmo
modelo de `RoomParameters`. Cada entrada declara palavra-chave, descrição, assinatura
tipada (`ArgType`: `objectId`, `itemId`, `flagId`, `roomId`, `animation`, `text`,
`boolean`), uma validação extra opcional e a função que executa (ou avalia). Uma
`InteractionCondition`/`InteractionEffect` guarda só o ponteiro para a entrada e os
argumentos em texto. O parser, o serializador, a validação por `ArgType` (inclusive
"o objeto existe na sala?"), a mensagem `Use <palavra> <args>` e a execução saem
todos do catálogo. Efeito novo = uma entrada + uma função.

Os efeitos de uma regra formam uma **sequência** executada por `game/ScriptRunner`.
Cada ação tem `start` e, se for bloqueante, um `update` chamado a cada tick até
devolver `done` (`ActionStatus::running | done`). Passos instantâneos rodam em
seguida no mesmo tick; um passo bloqueante (`say`, `walk_to`, `wait`,
`wait_animation`) segura a fila. O último passo nunca é esperado, porque nada depende
dele: uma regra que termina em `say` devolve o controle na hora, como antes.

As ações falam com a cena só por `game/ScriptHost` (sessão, fala, animação,
caminhada, troca de sala e as consultas de espera). `Game` implementa esse contrato
num adaptador privado (`Game::Host`), e um teste pode usar um host falso. Ação nova que
usa capacidades existentes = uma entrada + uma função no catálogo. Uma capacidade nova
da cena exige um método em `ScriptHost` e a implementação em `Game::Host`.

Enquanto um passo bloqueante segura a fila, `Game::inCutscene()` é verdadeiro: cliques
no mundo, `useItemOnObject` e `useItemAt` são ignorados, e o clique só pula a fala atual.
Uma UI futura (`ViewportUiConsumer`) deve consultar `inCutscene()` antes de agir.
`change_room` é adiado até o fim da sequência, para os passos seguintes ainda rodarem
na sala atual; trocar de sala cancela qualquer sequência em andamento. Mudanças de
colisão marcam a navegação como suja, e ela é reconstruída ao fim do passo ou antes de
um `walk_to`.

O mesmo runner é a base para eventos de entrada de sala e diálogos: uma fala linear já é
uma sequência de `say`.

### Diálogos com escolhas

Os diálogos da sala ficam no companheiro opcional `<sala>.dlg` (`content/DialogueFiles`),
carregado como `RoomDialogues` (definição imutável: nós, falas, escolhas com condições do
catálogo, `once` e `goto`). `Application` e o comando `load` do editor conferem com
`dialogueReferencesError` que todo `start_dialogue` do `.rules` aponta para um nó existente
antes de aplicar; `Game::setDialogues` valida o resto contra a sala.

`start_dialogue <no>` é uma ação bloqueante do catálogo. O `ScriptRunner` mantém uma **pilha
de sequências**: a regra que chamou `start_dialogue` fica esperando embaixo, e
`DialoguePlayer` (estado de execução, em `Game`) empilha por cima as falas do nó e, depois, os
efeitos da escolha. A ação nunca empilha sozinha: `ScriptHost::startDialogue` só registra o
pedido, e `Game::settleScript` o abre depois que a ação devolve, então nenhum passo é
reentrado. Fases: `lines` (falas do nó) → `choosing` (as escolhas cujas condições valem, na
ordem do arquivo) → `reply` (efeitos da escolhida; se houver `goto`, o último passo também é
esperado, para a resposta ser lida antes do próximo nó) → próximo nó ou fim. Sem escolhas
disponíveis, a conversa termina depois das falas. Quando a conversa termina, a regra de baixo
retoma no tick seguinte.

`inCutscene()` vale durante toda a conversa. Durante `choosing`, `ui/DialogueMenu` (um
`ViewportUiConsumer`) mostra as opções e chama `Game::chooseDialogueOption(i)`; nas outras
fases o clique cai em `Game::onClick` e só pula a fala. `change_room` dentro de um diálogo
espera a conversa e a regra que a abriu terminarem. Escolhas `once` ficam registradas em
`SessionState` (`sala/no/escolha`) e somem nas próximas conversas. Trocar de sala encerra a
conversa e descarta os diálogos da sala antiga; `Game::setDialogues` também encerra a conversa
em andamento, porque o `DialoguePlayer` aponta para a definição.

As regras são avaliadas na ordem declarada: a primeira regra cujas condições forem
todas verdadeiras executa seus efeitos em sequência e encerra a avaliação. Objetos
ocultos (`!isObjectVisible`) deixam imediatamente de ser desenhados em `SceneRenderer`,
deixam de bloquear `Navigation` e são ignorados no clique (`pickInteractiveObject`).

Para adicionar arte, configure um `SpriteClip` no conjunto de animações da sala
ou do objeto. A definição guarda o caminho relativo e o corte da imagem, sem carregar texturas. O
`SpriteLibrary` carrega PNGs sob `assets/sprites`, mantém as texturas em cache e
desenha o quadriculado de substituição quando o PNG ou corte falha. O cache é por
arquivo: edições no editor e mudanças de `SessionState` não o apagam. Ao entrar em outra
sala, `SpriteLibrary::beginRoom` descarta só as texturas que nenhuma das duas últimas salas
usou (ir e voltar entre duas salas não recarrega nada); `clear()` fica para os comandos `load` e `reload` do editor (PNG trocado em disco) e para reset do
dispositivo de render. Os clips são validados quando a sala é aplicada, então o desenho só
confere, em O(1), se o corte cabe no PNG carregado.

O ritmo dos frames vem do VSync (`SDL_SetRenderVSync`, ligado em `Renderer::initialize`); o
limitador por `sleep_until` de `Application::mainLoop` só marca o passo quando o VSync não está
disponível ou a janela não aparece (minimizada, coberta ou oculta: a simulação continua, nada é
desenhado), e limita a 240 fps um driver que ignore o VSync.

O jogo simula a 60 Hz e o render interpola posição, câmera e tempo de animação
(`AnimationPlayback::interpolatedElapsed`) entre os dois últimos ticks.

## Save/load de partida

O save guarda só o progresso, nunca definições nem simulação:

- **Tipos de domínio** (sem I/O): `SessionState::snapshot()` devolve um `SessionSnapshot` de
  valor com listas ordenadas (flags por id, itens na ordem do inventário, overrides por
  `sala + objeto`, escolhas `once` por `sala/no/escolha`), e `restore()` o valida e troca todo o
  progresso de uma vez (nada muda se falhar). `SavedGame` = `roomId` + pés + direção (`Facing`) + `SessionSnapshot`;
  `Game::savedGame()` o monta.
- **Formato** (`content/SaveFiles`), texto `chave = valor` como o `.room`: comentários `#`,
  `std::locale::classic()`, erro com número de linha, chaves desconhecidas, repetidas e IDs
  inválidos (`validIdentifier`) recusados.

  ```text
  version = 2                        # primeira chave; a leitura aceita 1..atual, a gravação usa a atual
  room = gallery                     # <id>.room
  feet = 220, 460
  facing = left                      # desde a versão 2, opcional (padrão down)
  flag.porta_aberta = 1              # inteiro != 0
  item = chave                       # repetível, em ordem
  object.first.red_box.visible = 0   # cada campo opcional
  object.first.red_box.solid = 0
  dialogue = gallery/mesa/gavetas    # repetível
  ```

  A gravação é determinística (o mesmo progresso gera os mesmos bytes). **Toda chave nova
  incrementa `SaveFiles::currentVersion`** e a leitura das versões anteriores continua (campo
  novo no padrão); como o leitor recusa chaves desconhecidas, um build antigo recusa o arquivo
  pela versão, com mensagem clara. Histórico: 1 = sala, pés, flags, itens, objetos, diálogo;
  2 = + `facing`. O save guarda só dados com ID estável e coordenadas da sala, nunca índices,
  o spawn de entrada ou estado de simulação: mudanças de geometria caem em `placePlayerNear`, e
  overrides/flags de IDs que deixaram de existir são inofensivos (ficam sem efeito).
- **Caminho**: `SDL_GetPrefPath(SaveFiles::organization, SaveFiles::application)` +
  `save.slot` (`SaveFiles::defaultSlot`, string liberada com `SDL_free`). Tudo recebe o
  caminho do arquivo, então os testes usam uma pasta temporária. Sem pasta gravável, o jogo
  roda sem save. Escrita por `TextFormat::writeTextAtomic` (`.tmp` + rename): uma falha mantém
  o save anterior.
- **Carga** (`Application::resumeSavedGame`, no `initialize`): `SaveFiles::resume` lê o save e
  carrega a sala por `RoomLibrary::loadBundle`. O `SessionState` é restaurado **antes** de a
  sala ser aplicada (`Game(Room, SessionState)`), porque a navegação depende de visibilidade e
  colisão; depois `Game::placePlayerNear` põe os pés (e a direção) onde estavam, ou no ponto caminhável mais
  próximo, ou fica no `playerStart`. Save ausente = novo jogo em `first.room`
  (`RoomLibrary::startRoomFile`). Sala do save que não carrega mais (removida ou renomeada numa
  atualização, arquivo quebrado) = o progresso é mantido e a partida retoma em `first.room`, no
  `playerStart` (`ResumeResult::warning`, log uma vez); o arquivo fica e o autosave da entrada na sala o
  regrava. Corrompido, de versão desconhecida, ou sala do save e `first.room` sem carregar =
  log uma vez, o arquivo vira `save.slot.bad` (nunca apagado) e novo jogo.
- **Quando grava**: `AutosavePolicy` guarda o último snapshot **estável**, capturado em
  `RoomEntered` e `CutsceneEnded` (consumidos em `Application::processGameEvents`) e no momento
  do autosave se não houver cutscene/diálogo. No meio de uma sequência vale o snapshot anterior
  a ela, para um puzzle nunca recarregar pela metade. Grava ao entrar numa sala
  (`RoomEntered` → `AutosavePolicy::checkpoint`, para um crash custar no máximo a sala atual),
  uma vez por ida ao background (`WILL_` e `DID_ENTER_BACKGROUND` chegam como o mesmo sinal) e
  ao sair (`SDL_EVENT_QUIT`, `TERMINATING`), e pula a escrita se o snapshot é o último que este
  processo gravou. O SDL pede que esses
  eventos de ciclo de vida sejam tratados num `SDL_AddEventWatch` (o sistema pode congelar o app
  logo depois); `Application::lifecycleWatch` faz isso na thread principal, e a cópia do evento
  que chega pelo `InputRouter` cai na mesma política sem gravar de novo.

Fora do escopo: vários slots, menu de save/load e nuvem.

## Evoluir diálogos, inventário visual e transições

Os cinco nomes de locomoção do jogador (`idle`, `left`, `right`, `up`, `down`)
são escolhidos em `Player.cpp`. Para animações de objetos acionadas por puzzles,
use o efeito declarativo `play_animation <objeto> <clip>` no `.rules` ou
`Game::playObjectAnimation` em C++; a conclusão de um clip sem repetição pode ser
consultada com `AnimationPlayback::finished`. Não use um novo valor de
`SceneObjectType` para cada animação: esse enum expressa apenas se a base autoral
do objeto bloqueia o caminho por padrão.

Para uma futura UI visual de inventário, siga o modelo de `ui/DialogueMenu`: um
`ViewportUiConsumer` registrado em `InputRouter` operando na viewport fixa `1280×720`,
com o layout no próprio módulo e o desenho em `graphics/`. A UI de inventário pode ler `game.session().items()` (só leitura; mudanças por `Game::changeSession`) e disparar
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
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Os testes cobrem movimento, perspectiva, desvio, quinas, salas panorâmicas,
câmera fixa e suave com clamp nas bordas, Walk-to-Interact (aproximação, cancelamento
no meio do caminho, disparo imediato quando já posicionado e picking por profundidade),
coleta de itens, diálogos com escolhas condicionais (`once`, `goto`, retomada da regra,
menu por clique/setas/gamepad e round-trip do `.dlg`), puzzles/NPCs condicionais com desbloqueio dinâmico de `Navigation`,
save/load (round-trip determinístico, retomada do puzzle da chave, arquivos ruins com linha,
quarentena `.bad`, escrita atômica e autosave uma vez por ida ao background), persistência de `SessionState` entre salas e durante edições do `DebugEditor` (`set`,
`move`, `undo`, `reset`), parser/serialização/validação por linha de `.rules`,
configurações inválidas, entrada multiplataforma e independência de resolução (720p,
1080p, 2K, 4K e Ultrawide). Os testes de sprites cobrem cortes, animação, seleção de
direção, persistência e imagem de substituição. Os testes gráficos usam o driver
virtual e o renderizador de software do SDL. As verificações também ficam ativas
em Release.
