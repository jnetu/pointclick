# Revisão geral e roadmap da engine

**Data:** 09/11/2026 · **Commit revisado:** `bd26b9e` · **Escopo:** todo `src/`, `cmake/`, `assets/`,
`docs/` e a estrutura dos testes.

Este projeto é a **engine**; história, personagens e música ficam nos jogos construídos sobre
ela. Aqui só entram mecânicas e *playgrounds* pequenos para testá-las. As listas abaixo seguem
esse recorte.

Convenção dos itens: **o quê** · evidência · onde mexer · esforço estimado. "Toca" lista os
arquivos que mudam, para medir se a feature entra sem espalhar edições.

---

## 1. O que foi verificado

| Verificação | Resultado |
| --- | --- |
| `cmake --preset debug` + build + `ctest` | 4/4 testes passam |
| Build com ASan + UBSan (pasta temporária) | 4/4 passam, nenhum relatório dos sanitizers |
| Build Release com `-DPOINTCLICK_ENABLE_TOOLS=OFF` (loja) | compila, 3/3 passam, executável de 517 KB |
| `-Wconversion -Wshadow -Wold-style-cast` | 11 avisos triviais (`Room.cpp:26-27`, `Navigation.cpp:65`, `SpriteLibrary.cpp:88-89`, `InputSystem.cpp`, 2 nos testes) |
| Fuzz dos parsers sob ASan/UBSan (26.500 mutações dos assets reais em `.room`, `.rules`, `.dlg`, `.actor`, `save.slot`, executando o jogo nas entradas aceitas) | nenhum crash, nenhum comportamento indefinido |
| Harness de casos de borda (14 sondas ligadas às bibliotecas do projeto) | confirmou os bugs da seção 3.1 |
| Captura headless de um frame de `first.room` (renderizador de software) | confirmou texto cortado e desenho de diagnóstico em produção |

**Não verificado:** execução em janela/GPU real, Windows, Linux, Android, iOS e consoles (não há
projetos de empacotamento), nem medição em hardware fraco. Os itens marcados *por leitura* não
foram reproduzidos.

---

## 2. Análise da engine

### 2.1 Modularidade

**O que está sólido.** Um target por camada com o linker recusando dependência proibida; o teste
`pointclick_layer_includes` passa; `RoomParameters` e `ActionCatalog` são realmente
*uma linha + uma função*; saída do domínio só por `GameEvent`; estado separado em definição ×
simulação × sessão; carga transacional (`Scene::apply` monta a navegação à parte e só então
troca). O jogo compila sem o editor.

**Onde uma feature nova ainda espalha edições** (o que mais importa para o objetivo de "entrar sem
mexer em tantos arquivos"):

| Ponto | Hoje exige tocar | Proposta |
| --- | --- | --- |
| Gatilho novo (`look`, `talk`, `on_enter`) | enum `InteractionTriggerKind`, `InteractionTrigger::matches`, `if/else` em `InteractionFiles::parse` (linhas 62-67) e em `save`, método novo em `Game` | `TriggerCatalog` no modelo do `ActionCatalog` (palavra-chave + assinatura tipada) |
| Campo novo de objeto (`name`, `approach`, `baseline`) | cadeia de `if` em `RoomFiles::load`, escrita manual em `RoomFiles::save`, `Room::validationError`, editor | `ObjectFieldCatalog` (chave, leitura, escrita, validação), como `RoomParameters` |
| Comando novo do editor | cadeia de `if` em `DebugEditor::submit` + texto de ajuda fixo em `DebugOverlay.cpp` | `CommandCatalog` (nome, uso, função); `help` e a ajuda do painel saem dele |
| UI de gameplay nova (inventário, menu) | `Renderer::render(..., const DialogueMenu*)` ganha um parâmetro por UI, mais `Application` | registro de desenhistas de UI no `Renderer` (como `RenderOverlay`), ordenados por camada |
| Campo novo no save | `SessionState`, `SessionSnapshot` (+ `validationError`, `snapshot`, `restore`), `SaveFiles::parse` e `serialize` | tabela de campos do save (chave → ler/gravar) ou, no mínimo, um teste que falha quando um campo do snapshot não é serializado |
| Ator além do player | `Room` carrega `playerAnimations`, `playerBaseSize`, velocidades e `playerStart` | definição `Actor` própria (seção 4, M7) |
| Jogo que precisa de uma ação própria | editar `ActionCatalog.cpp` da engine (vetor estático) | catálogos registráveis (seção 5, L1) |

### 2.2 Multiplataforma

Desktop está bem encaminhado: leitura por `SDL_IOStream`, caminhos UTF-8, escrita atômica em
`SDL_GetPrefPath`, toque sem mouse sintético, gamepad com hotplug, ciclo de vida tratado em
*event watch*, teclado virtual sob demanda. Riscos concretos antes de um primeiro build mobile:

- ✅ (C8, sem teste em Android) `RoomLibrary::open` usa `std::filesystem::exists` para o `player.actor`
  ([RoomLibrary.cpp:16](../src/content/RoomLibrary.cpp)); assets dentro do APK não aparecem no
  sistema de arquivos, então no Android a arte compartilhada não seria carregada e toda sala
  falharia em "Animacao obrigatoria do player" (*por leitura*).
- Não existe projeto Android/Xcode, nem `SDL_MAIN_USE_CALLBACKS` (o loop bloqueante não serve a
  iOS/Web sem adaptação), nem *safe area*, nem orientação travada.
- `writeTextAtomic` usa `std::ofstream` + `std::filesystem::rename`; consoles pedem a API de
  armazenamento da plataforma (`SDL_Storage`) e nem todos têm `std::filesystem` completo.
- ✅ (C9) `SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)`: o áudio ainda não é usado e, num ambiente sem
  driver de áudio (CI, contêiner Linux), a falha desse subsistema impede o jogo inteiro de abrir.

### 2.3 Eficiência

Medido em Release neste Mac (números para comparar no futuro, não como requisito):

| Medida | Resultado |
| --- | --- |
| `Game::tick` com 300 objetos animados | 0,0002 ms |
| Pathfinding por clique, sala 1280×720 | média 0,03 ms, pior 0,09 ms |
| Pathfinding, sala 4096×4096 com 104 obstáculos | média 0,9 ms, pior 1,6 ms |
| Pathfinding, sala 8192×8192 com 419 obstáculos | média 3,0 ms, pior 7,3 ms |
| Pathfinding, corredor em zigue-zague 8192×8192 (caso patológico) | **400 ms** em um clique (24 frames) |
| `Scene::pick` com 300 objetos × 600 regras | 0,09 ms por consulta (varredura linear dupla) |

A simulação não é gargalo. Os riscos estão na apresentação e em casos extremos:

- ✅ (D2) **Sem VSync.** `Renderer::initialize` nunca chama `SDL_SetRenderVSync`; o ritmo depende só do
  `sleep_until` ([Application.cpp:291](../src/core/Application.cpp)).
- **PNG carregado no primeiro desenho**, de forma síncrona (`SpriteLibrary::imageFor`): engasgo
  no primeiro frame de cada sala quando houver arte de verdade.
- **Suavização da rota é O(n²)** com `clearLine` sobre caixas grandes
  ([Navigation.cpp:202](../src/game/Navigation.cpp)); é o que explica os 400 ms.
- ✅ (D1) **Overlay de diagnóstico** roda todo frame em produção (consultas ao SDL, 12 textos
  formatados e um `substr`).
- Sem *culling*: tudo é enviado ao SDL mesmo fora da câmera (irrelevante hoje).

---

## 3. Curto prazo (horas a dias)

### 3.1 Bugs confirmados

✅ = corrigido em 09/11/2026, com teste de regressão que falha sem a correção (C1 e C5 em
`tests/GameTests.cpp`; C3 em `GameTests.cpp` e `EditorTests.cpp`; C6 em `EditorTests.cpp`).
Segundo lote, no mesmo dia: C9, C10 (`AutosavePolicy::checkpoint`, testado em `GameTests.cpp`),
D1 (testes em `EditorTests.cpp` e `SpriteTests.cpp`) e D2. C9, D2 e a gravação em si do C10
ficam em `Application`, sem teste automático: foram conferidos rodando o executável headless.
O VSync numa janela com GPU real ainda não foi conferido.
Terceiro lote, em 10/11/2026: C7 (callback `setArtReloader`, comando `reload`; testes em
`EditorTests.cpp` e `SpriteTests.cpp`), C8 (`ActorFiles::loadOptional`; o comportamento no
desktop está testado, o caso Android em si continua sem aparelho para conferir), C11 (teste em
`GameTests.cpp`) e D4 (`relatorio.md` e a auditoria foram para `docs/historico/`).
Quarto lote (higiene), também em 10/11/2026: D3 (`Game::changeSession`, teste em `GameTests.cpp`),
D5 (eram 19 avisos com as flags estritas, não 11; zerados no clang e `-Wconversion -Wshadow`
ligados em todos os targets; conferido depois no GCC 16.2, também sem avisos, com
`-Wno-missing-field-initializers` só nele, por causa dos inicializadores designados dos testes;
MSVC não foi conferido), D6 (`game/PlayerClips.hpp`, com
regra nova em `cmake/CheckLayerIncludes.cmake`), D7 (`TextFormat::number`, testes em
`GameTests.cpp` e `EditorTests.cpp`) e D8 (teste em `EditorTests.cpp`).

| # | Problema | Evidência | Onde / correção | Esforço |
| --- | --- | --- | --- | --- |
| C1 ✅ | **Softlock de cutscene:** `wait_animation` nunca termina num clip com `loop = 0` e `fps = 0` (imagem estática). A sala é aceita pela validação. | Sonda: após 125 s e cliques, `inCutscene() == true`. | `AnimationPlayback::finished` exige `fps > 0` ([Animation.cpp:36](../src/game/Animation.cpp)). Tratar `fps <= 0` como terminado em `Scene::animationFinished` + teste. | 30 min |
| C2 | **Fala ilegível:** texto em 4× da fonte de debug, sem quebra de linha, começando em x=20. Cabem 39 caracteres; **10 das 15 falas dos assets são cortadas**, e o painel de diagnóstico cobre o começo. `\n`, documentado em `GAME_DESIGN.md:313`, não quebra linha (limite do `SDL_RenderDebugText`). O texto ainda deriva 5 px/s. | Captura headless: de "Encontrei uma chave de latao escondida na caixa azul!" só aparece "chave de latao escondida n". | Paliativo: quebra por palavras em `graphics/` (ASCII, escala 2), centralizado acima do player, sem deriva em `FloatingText`. Solução real: M1. | ½ dia |
| C3 ✅ | **BOM UTF-8 quebra todos os formatos** com mensagem enganosa na linha 1 (arquivo salvo no Bloco de Notas do Windows). CRLF já funciona. | Sonda: `.room`, `.rules`, `.dlg` e save recusados. | Remover o BOM em `TextFormat::readFile`/`readOptionalFile` (um lugar só) + teste por formato. | 30 min |
| C4 | **Estado visual de objeto não persiste:** `play_animation porta open` volta ao clip inicial ao sair e voltar da sala e ao carregar o save, enquanto flag e colisão continuam "aberta". | Sonda: `anim=idle`, `flag=1`, `solid=0`; o save não contém a animação. | Campo `animation` em `ObjectStateOverride` + efeito persistente (`set_animation <obj> <clip>`, ou `play_animation` de clip sem loop passa a persistir); `ObjectAnimations::rebuild` parte do clip salvo já no último quadro. Toca `SessionState`, `SaveFiles` (versão 3), `ActionCatalog`, `Scene::apply`, docs, testes. | 1–2 dias |
| C5 ✅ | **Fala atravessa a troca de sala:** `change_room b` seguido de `say` (último passo, não esperado) deixa o texto na sala nova. | Sonda: fala visível após `applyRoom`. | `speech_.hide()` em `Scene::apply` quando `resetPlayer` + teste. | 15 min |
| C6 ✅ | **Editor ativo com painel oculto engole setas/Enter:** o menu de diálogo não responde a teclado/gamepad enquanto se testa com TAB. | Sonda: após Baixo+Enter ainda `awaitingDialogueChoice()`. | [DebugEditor.cpp:115](../src/debug/DebugEditor.cpp): devolver `false` para ações que o painel oculto não usa. | 15 min |
| C7 ✅ | **`load` do editor não recarrega PNGs**, ao contrário do que diz `GAME_DESIGN.md:190`; PNG ausente fica em cache como nulo até duas trocas de sala. | Leitura: só `invalidateGraphics` chama `SpriteLibrary::clear`. | Interface pequena `ToolHost { reloadArt(); }` dada ao `DebugEditor` pelo `Application`; `load` e um comando `reload` a chamam. | 1–2 h |
| C8 ✅ | **Android: arte do player não carrega** (*por leitura*, ver 2.2). | — | `RoomLibrary::open`: tentar abrir pelo SDL e tratar falha de abertura como ausência, sem `std::filesystem::exists`. | 30 min |
| C9 ✅ | **Sem driver de áudio (CI, contêiner Linux) o jogo não abre** (*por leitura*); o áudio nem é usado ainda. | [Application.cpp:179](../src/core/Application.cpp). | Tirar `SDL_INIT_AUDIO` até existir `audio/`; depois, iniciar o subsistema à parte e seguir sem som se falhar. | 10 min |
| C10 ✅ | **Progresso perdido em crash:** o autosave só é gravado ao ir para background ou sair. | Leitura de `processGameEvents`. | Gravar também em `RoomEntered` (a política já pula escrita sem mudança). | 30 min |
| C11 ✅ | **Clique num objeto já ao alcance faz o player "ajeitar"** até o centro da célula antes de interagir. | Sonda: a 10 px do ponto, anda de (889,629) para (880,624). | `Scene::approach`: se já está ao alcance e a rota é menor que uma célula, resolver na hora. | 1 h |
| C12 | **Uso de item sem regra não dá resposta:** o player anda até o objeto e nada acontece. | Sonda: `useItemOnObject` devolve `true`, sem fala nem evento. | Ver F4 (regras curinga). | — |

### 3.2 Dívidas e higiene

- ✅ **D1 — Desenho de debug em produção** (dívida do `CLAUDE.md`): linha da rota e cruz do destino
  ([SceneRenderer.cpp:20,55](../src/graphics/SceneRenderer.cpp)), `DiagnosticsOverlay` sempre
  ([Renderer.cpp:96](../src/graphics/Renderer.cpp)) e a dica "DEBUG / F1" **mesmo em build sem
  ferramentas**. Correção modular: `DiagnosticsOverlay`, rota e destino viram `RenderOverlay` em
  `src/debug/` (leem `const Scene&`), ligados por flags do editor (`show route 1`), desligados
  por padrão. `SceneRenderer` fica só com a cena; builds de loja perdem tudo sem `#if`. Toca
  `graphics/SceneRenderer.cpp`, `graphics/Renderer.cpp`, `debug/`, `CMakeLists.txt`,
  `tests/EditorTests.cpp`. *½ dia.*
- ✅ **D2 — VSync:** `SDL_SetRenderVSync(renderer_, 1)` na criação; manter o `sleep_until` como
  reserva quando o VSync falhar. Pausar o render com a janela minimizada/oculta. *1 h.*
- ✅ **D3 — `Game::session()` não-const é público:** mexer na sessão por fora deixa a navegação
  velha (sonda: `isObjectSolid == 0` e a célula continua bloqueada). Hoje só os testes usam.
  Expor só a versão `const` e criar comandos no `Game` que marcam a navegação suja (os comandos
  de F2 precisam disso). *2–3 h com o ajuste dos testes.* Feito com um comando só,
  `Game::changeSession(função)`, que compara quais objetos da sala bloqueiam o chão antes e
  depois da mudança. A rota já em curso ainda não é revalidada (M5).
- ✅ **D4 — Documentação defasada:** `ARCHITECTURE.md:152-154` diz que não há pasta gravável para
  saves; `GAME_DESIGN.md` promete `\n` em `say` e recarga de PNG com `load`; `relatorio.md`
  (20 ticks, `pointclick_core`) e `AUDITORIA_TECNICA_2026-09-28.md` descrevem um projeto que não
  existe mais (arquivar em `docs/historico/` ou remover); `assets/sprites/README.md` tem um
  caminho absoluto `/Users/joao/...` e formatação quebrada. *1 h.*
- ✅ **D5 — Avisos estritos:** corrigir os 11 e ligar `-Wconversion -Wshadow` nos targets. *1 h.*
- ✅ **D6 — `Room.cpp` inclui `Player.hpp`** só pelos nomes dos clips de locomoção: mover as
  constantes para um header de definição. *15 min.*
- ✅ **D7 — Gravação de sala ruidosa:** `setprecision(max_digits10)` grava `53.3333321`; usar o
  formato mais curto que faz *round-trip* (`std::to_chars`) em `RoomFiles` e `SaveFiles`. O
  `save` do editor também descarta os comentários do designer (documentar). *1–2 h.*
- ✅ **D8 — Ponteiro fora da janela:** tratar `SDL_EVENT_WINDOW_MOUSE_LEAVE` para `pointerInside`
  não ficar preso (vai importar no hover). *30 min.*

### 3.3 Features pequenas de alto valor

- **F1 — Opções de linha de comando para teste:** `--new-game` (ignora o save), `--no-save`,
  `--room <id> [spawn]` e `--check` (valida todas as salas, `.rules`, `.dlg` e
  `transitionsError`, sai com código de erro; serve a CI). Hoje toda execução retoma o
  `save.slot` da pasta do usuário, o que atrapalha testar mecânicas do zero. Toca `main.cpp`,
  `Application`. *½–1 dia.*
- **F2 — Comandos de sessão no editor:** `flag <id> <n>`, `give`/`take <item>`,
  `show`/`hide <obj>`, `use <item> <obj>`, `goto <sala> [spawn]`, `newgame`, `state` (lista
  flags, itens e overrides). Sem isso a regra `[use_item brass_key red_box]` dos assets **não
  pode ser disparada no jogo**, porque não existe UI de inventário. Fazer junto com o
  `CommandCatalog` (2.1) para o `help` sair da tabela. *1–2 dias.*
- **F3 — Playgrounds por mecânica + teste de percurso:** os assets atuais não exercitam em jogo
  real `change_room`, `[spawn]`, câmera (as duas salas têm 1280 de largura), sprites/animação
  de objeto, `walk_to`, `wait` nem `wait_animation`. Criar salas pequenas (ver seção 7) e um
  teste headless que carrega os assets e joga cada sala com uma lista de cliques até um estado
  esperado. *2–3 dias.*
- **F4 — Regras curinga:** `[use_item * <obj>]`, `[use_item <item> *]` e `[interact *]` como
  resposta padrão ("Isso não funciona."). Toca `InteractionTrigger::matches`, parser, docs,
  testes. *½ dia.*
- **F5 — Contadores nas regras:** `add_flag <flag> <n>`, `flag_at_least <flag> <n>`,
  `flag_equals <flag> <n>`. `SessionState::incrementFlag` já existe; são três entradas no
  catálogo. *2 h.*
- **F6 — `face <direção>`:** um método em `ScriptHost` + uma entrada no catálogo. *2 h.*
- **F7 — `flip_x` nos clips:** `SpriteLibrary::draw` já aceita `flipHorizontally`; falta o campo
  em `SpriteClip`/`SpriteFields` para reaproveitar a folha `right` como `left`. *2 h.*

---

## 4. Médio prazo (semanas)

Ordem sugerida: M1 e M2 primeiro (sem texto e fundo, nenhuma outra mecânica é avaliável na tela),
depois M3–M6 (o núcleo de um point & click), depois o resto.

- **M1 — Texto de verdade.** `graphics/TextRenderer` com SDL3_ttf (ou fonte bitmap UTF-8),
  cache de glifos e quebra de linha; layout da fala em `ui/` (ancorada ao falante, dentro da
  viewport); `LineSpoken{speaker, text}`; `say` com falante opcional. Substitui o paliativo de
  C2 e destrava acentos. Toca: `graphics/` (novo), `ui/` (novo), `GameEvent.hpp`,
  `ActionCatalog.cpp`, `cmake/Dependencies.cmake`.
- **M2 — Fundo e camadas.** Hoje a sala é só cor chapada + guias, e objeto sem clip desenha um
  retângulo colorido: não dá para ter *hotspot* invisível sobre uma arte de fundo. Acrescentar
  `[layer <id>]` (arquivo, `scroll_factor` para parallax, atrás/à frente, `baseline`),
  `[hotspot <id>]` invisível e `baseline` por objeto, com picking e render usando a mesma função
  de profundidade. Toca: `Room`, `RoomFiles` (via `ObjectFieldCatalog`), renderer de camadas
  novo em `graphics/`, `Interaction.cpp`.
- **M3 — Verbos, hover e cursor.** `TriggerCatalog` (`look`, `talk`, `pick_up`...); `input/` com
  ação secundária (botão direito, toque longo) e *release*; `Scene::hoveredObject()` recalculado
  por tick a partir do ponto na viewport (hoje o ponto no mundo fica velho quando a câmera
  rola); `name`, `approach = x, y` e `facing` por objeto; rótulo de hover e cursor em `ui/`.
  Montar o índice `objeto → regras` no commit da sala para o hover custar O(objetos).
- **M4 — Inventário.** Catálogo `assets/items.items` (nome, ícone, descrição), `ui/InventoryPanel`
  como `ViewportUiConsumer`, item selecionado no cursor, gatilho `[combine <a> <b>]`, eventos
  `ItemAdded`/`ItemRemoved`. A API do domínio (`useItemOnObject`, `useItemAt`) já existe.
- **M5 — Gatilhos de sala.** `[on_enter]`, `[on_first_enter]`, `[on_exit]`, zonas
  (`[zone <id>]` + `[enter_zone <id>]`) e temporizadores, todos no mesmo `ScriptRunner`. Com
  scripts que rodam enquanto o player anda, passa a valer **revalidar a rota em curso** quando a
  colisão muda (hoje `refreshNavigation` só reposiciona quem ficou dentro de um obstáculo).
- **M6 — Scripts mais expressivos.** Sequências nomeadas (`[sequence <id>]` + `run <id>`);
  `global.rules` para regras compartilhadas; referência a objeto de outra sala
  (`hide_object sala.objeto`); **cutscene pulável** (Esc avança `say`/`wait`/`walk_to` até o
  fim; hoje `wait 3600` prende o jogador por uma hora); sequências em segundo plano; aviso de
  validação para nó de diálogo sem caminho de saída.
- **M7 — Atores e NPCs.** Definição `Actor` (clips, tamanho, velocidades) fora de `Room`;
  `[actor <id>]` na sala; `Player` vira "o ator controlado"; ações `walk_actor`, `say <ator>`,
  animação de fala; atores entram na ordenação por profundidade e no picking. Manter a leitura
  das chaves `player.*` antigas do `.room`.
- **M8 — Áudio.** `src/audio/AudioSystem` consumindo `GameEvent` (`SoundRequested`,
  `MusicRequested`), ações `play_sound`, `play_music`, `stop_music`; pausa em background;
  volumes nas configurações. Target novo `pointclick_audio`, dependendo só de `game`.
- **M9 — Transições.** *Fade* como estado temporizado em `graphics/`; a troca de sala passa a ter
  duas fases (escurece → carrega → clareia) com a entrada bloqueada; pré-carregar as texturas da
  sala em `RoomEntered`, durante o escuro.
- **M10 — Manifesto e configuração do app.** `assets/game.manifest` (título, organização e nome
  para a pasta de saves, sala inicial, lista de salas, idiomas). Hoje estão fixos no código
  `first.room`, "Point & Click" e `SaveFiles::organization/application`. Validação cruzada de
  tudo no boot em builds de desenvolvimento (e no `--check`).
- **M11 — Entrada.** Tabela de *bindings* remapeável (hoje é um `switch` em
  `InputSystem::translateEvent`), repetição no D-pad, ciclo entre hotspots com *snap* no
  gamepad.
- **M12 — Menus e configurações.** Pausa, slots de save com salvar/carregar manual, arquivo de
  configurações no *pref path* (volume, tela cheia, idioma, velocidade do texto), alternar tela
  cheia. Depende do registro de UIs (2.1).
- **M13 — Localização.** `@chave` em `say` e nos textos de escolha, `text/<idioma>.strings`,
  chaves ausentes acusadas no carregamento.
- **M14 — Navegação.** Limitar a suavização (parar no primeiro candidato bloqueado ou limitar o
  alcance) e usar só a regra de quina dentro do A*/BFS; ponto caminhável mais próximo
  considerando alcance; área caminhável por polígonos ou máscara PNG rasterizada na mesma grade.
- **M15 — Save.** Backup `save.slot.bak` antes de substituir, metadados (data, tempo de jogo,
  sala), slots; manter a regra "chave nova = versão + 1".
- **M16 — Editor.** Arrastar objetos com o mouse; criar/remover objetos, spawns e hotspots;
  overlays por flag (grade de navegação, hotspots, pontos de aproximação); recarga automática de
  `.rules`/`.dlg` ao salvar o arquivo; histórico de comandos.
- **M17 — Qualidade.** Migrar para doctest (hoje são 920 `assert` em três executáveis; a
  primeira falha aborta sem nomear o caso); CI em macOS/Linux/Windows com ASan/UBSan; o fuzz
  desta revisão como target; teste de frame de referência; `--check` dos assets na CI.

---

## 5. Longo prazo (meses)

- **L1 — Engine como base reutilizável.** É o que o objetivo do projeto pede e hoje não existe:
  a engine é um executável + uma pasta de assets. Passos: separar `engine/` de `playground/`
  (assets e salas de teste); exportar targets CMake (`pointclick::engine`) para um jogo consumir
  por `FetchContent`/`add_subdirectory`; **catálogos registráveis** (ações, condições, gatilhos,
  comandos do editor) para o jogo acrescentar vocabulário sem editar a engine; `Application`
  configurável (`AppConfig` + pontos de registro de UI, overlays e consumidores de evento);
  projeto-modelo de jogo; versionamento dos formatos de conteúdo; guia "como criar um jogo".
  Jogos só de dados continuam possíveis apontando o binário para outra pasta de assets.
- **L2 — Plataformas.** `SDL_MAIN_USE_CALLBACKS` (`Application` já separa `initialize`,
  `processEvents` e `shutdown`; falta extrair uma iteração do `mainLoop`); projeto Android (assets no APK, orientação, botão
  voltar, ciclo de vida em aparelho); projeto iOS (*safe area*); Web/Emscripten (saves em
  IDBFS); consoles. Matriz de aparelhos de teste.
- **L3 — Abstração de I/O.** `content/AssetLocator` + `SDL_Storage` (título para leitura,
  usuário para escrita) no lugar de `std::filesystem` e `std::ofstream`; pacotes de assets;
  carregamento assíncrono com tela de carregamento.
- **L4 — Render.** Atlas de texturas, *render targets* para transições e efeitos, tinta/luz
  ambiente por região, partículas simples, política de resolução (escala inteira opcional para
  pixel art, assets em mais de uma densidade).
- **L5 — Ferramentas.** Editor de salas mais completo ou pipeline de importação (Tiled,
  Aseprite); depurador de scripts (passo a passo, inspeção de flags); visualização do grafo de
  diálogo; gravação e reprodução determinística de entrada para regressão.
- **L6 — Scripting avançado.** Variáveis, expressões e blocos `if/else`; decidir se a lógica de
  jogo mais complexa fica declarativa ou vai para uma linguagem embutida atrás do `ScriptHost`.
  Adiar enquanto o catálogo der conta.
- **L7 — Recursos narrativos da engine.** Voz por linha, log de falas, legendas configuráveis,
  conquistas e telemetria como consumidores de `GameEvent`.
- **L8 — Acessibilidade.** Escala de texto, alto contraste, destacar todos os hotspots,
  remapeamento completo.
- **L9 — Orçamento de desempenho.** Perfil em hardware fraco (tempo de boot, memória de
  texturas, bateria), render sob demanda quando nada muda, instrumentação opcional.
- **L10 — Navegação avançada.** Polígonos com *funnel*, regiões com escala própria, vários
  andares, obstáculos móveis (NPCs), máscaras de *walk-behind*.

---

## 6. Dependências entre os itens

```text
C2 (paliativo) ─► M1 texto ─┬─► M3 hover/verbos ─► M4 inventário ─► M12 menus
D1 flags de debug ──────────┤                 └─► M13 localização
M2 fundo/camadas/hotspots ──┘
D3 + F2 (comandos de sessão) ─► F3 playgrounds + teste de percurso ─► M17 CI
C4 (estado persistente de objeto) ─► M15 save
M5 gatilhos ─► M6 scripts ─► M7 atores
M10 manifesto ─► L1 engine reutilizável ─► L2 plataformas ─► L3 I/O
```

## 7. Playgrounds sugeridos

Uma sala pequena por mecânica, com as regras mínimas, coberta pelo teste de percurso (F3):

| Sala | Exercita |
| --- | --- |
| `first` ↔ `gallery` ligadas por uma porta | `change_room`, `[spawn]`, `facing`, autosave em `RoomEntered` |
| `wide` (2560 de largura) | câmera suave e *clamp*, clique convertido com a câmera deslocada |
| `door` | objeto com sprites, `play_animation`, `wait_animation`, estado persistente (C4) |
| `lever` | sequência longa: `walk_to`, `wait`, `say`, modo cutscene, pular fala |
| `maze` | obstáculos densos, destino inalcançável, custo de rota (M14) |
| `talk` | diálogo com `once`, `goto`, condições, `change_room` dentro da conversa |
| `items` | pegar, usar item em objeto, regra curinga (F4), contadores (F5) |
