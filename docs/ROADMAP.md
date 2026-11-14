# Revisão geral e roadmap da engine

**Data:** 10/11/2026 · **Commit revisado:** `65bf916` · **Escopo:** todo `src/`, `cmake/`, `assets/`,
`docs/` e a estrutura dos testes. Substitui a revisão de 09/11/2026 (commit `bd26b9e`); os
identificadores dos itens (C, D, F, M, L) foram mantidos, porque o código e os outros documentos
os citam.

**Atualização de 11/11/2026:** C13, D9, D10 e D11 foram concluídos (mudanças feitas sobre
`65bf916`) e passaram para a seção 3.1. O resto do documento continua sendo a revisão de 10/11.

**Segunda atualização de 11/11/2026:** D12 (erros de sala com linha), F8 (aviso de arte ausente)
e C4 (animação de objeto que persiste, save versão 3) foram concluídos sobre `f1af9f1` e passaram
para a seção 3.1. Com eles o `ctest` tem 10 testes (9 na build sem ferramentas): debug 10/10,
ASan + UBSan 10/10 sem relatório, Release com `-DPOINTCLICK_ENABLE_TOOLS=OFF` 9/9, nenhum aviso
do compilador. A tabela da seção 1 continua registrando os números da revisão de 10/11.

**Terceira atualização de 11/11/2026:** a maior parte do M17 foi concluída sobre `344ac15` e
passou para a seção 3.1: testes em doctest (62 casos nomeados), `.github/workflows/ci.yml`
(macOS, Linux clang/gcc, Windows MSVC, ASan + UBSan, build de loja), presets `asan` e `store` e o
fuzz dos parsers como target opcional. **A CI foi escrita e conferida comando a comando no
macOS, mas ainda não rodou no GitHub Actions**: Linux, Windows/MSVC e o próprio workflow seguem
sem prova (seção 3.3).

**Atualização de 13/11/2026:** F2 (comandos de sessão no editor) foi concluído junto com o
`CommandCatalog` da seção 2.1 e com o modo headless `--exec`/`--exec-file`, sobre `bb818fb`, e
passou para a seção 3.1. O `ctest` tem 13 testes (9 na build sem ferramentas): debug 13/13,
ASan + UBSan 13/13, loja 9/9, sem avisos do compilador.

Este projeto é a **engine**; história, personagens e música ficam nos jogos construídos sobre
ela. Aqui só entram mecânicas e *playgrounds* pequenos para testá-las. As listas abaixo seguem
esse recorte.

Convenção dos itens: **o quê** · evidência · onde mexer · esforço estimado. "Toca" lista os
arquivos que mudam, para medir se a feature entra sem espalhar edições.

---

## 1. O que foi verificado nesta revisão

| Verificação | Resultado |
| --- | --- |
| `cmake --preset debug` + build + `ctest` (clang, SDL3 3.4.20 e SDL3_ttf 3.2.2 do sistema) | 8/8 testes passam, nenhum aviso com `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` |
| Build com ASan + UBSan (pasta temporária) | 8/8 passam, nenhum relatório dos sanitizers |
| Build Release com `-DPOINTCLICK_ENABLE_TOOLS=OFF` (loja) | compila sem avisos, 7/7 passam, executável de 583 KB, nenhum símbolo de `DebugEditor`/`DebugOverlay`/`DiagnosticsOverlay` no binário (`nm`) |
| `pointclick --check` nos assets e na pasta quebrada (pelo `ctest`) | passa nos assets; falha de propósito em `tests/fixtures/broken_rooms` |
| Leitura de todo `src/` contra os contratos do `AGENTS.md` | cada item ✅ da revisão anterior foi conferido no código; desvios novos nas seções 2 e 4 |
| Sonda de desempenho em Release, ligada a `pointclick_game` | números da seção 2.3; **zero alocações de heap** em `Game::tick`, andando ou parado |
| Sonda de overflow sob UBSan | confirmou o bug C13 (corrigido em 11/11, seção 3.1) |

**Não verificado nesta revisão:** execução em janela/GPU real, Windows/MSVC, Linux, Android, iOS
e consoles (não há projetos de empacotamento nem CI), medição em hardware fraco, e o fuzz dos
parsers (rodado na revisão de 09/11, não repetido; os parsers mudaram pouco desde então, mas
ganharam `ArgType::integer`, `facing`, curingas e `flip_x`). As alocações do lado do render foram
conferidas só por leitura. A lista de verificações em aberto está na seção 3.3.

---

## 2. A proposta ainda está sendo respeitada?

Resposta curta: **sim nas três frentes para o que existe hoje no desktop**. Modularidade e
desempenho estão garantidos por build e teste; multiplataforma está respeitada no código, mas
continua sem nenhuma prova fora do macOS. O que falta é o que o roadmap já previa, mais os
desvios pequenos listados abaixo.

### 2.1 Modularidade

**Respeitada.** Um target por camada com o linker recusando dependência proibida; o teste
`pointclick_layer_includes` passa; a build de loja não carrega nenhum código de ferramenta;
`pointclick_tests` linka só `content` + `game`. Saída do domínio só por `GameEvent` (nenhum
`consumeX()`); `Game::session()` é só leitura e as mudanças externas passam por
`Game::changeSession`; carga transacional em `Scene::apply`. Os catálogos seguraram a prova das
últimas features: `add_flag`, `flag_at_least`, `flag_equals` e `face` entraram como entradas do
`ActionCatalog` (mais um `ArgType` e, no `face`, um método do `ScriptHost`), sem tocar no parser
nem no serializador.

**Desvios encontrados** (pequenos, viraram itens). Os três já foram corrigidos (seção 3.1): a
base de colisão vermelha que `EntityRenderer::drawObject` pintava também na build de loja (D10),
o `ARCHITECTURE.md` atrás do catálogo e da ordem das regras curinga (D9) e os erros de
`Room::validationError`, que chegavam ao designer sem número de linha, ao contrário de `.rules`
e `.dlg` (D12).

**Onde uma feature nova ainda espalha edições** (o que mais importa para o objetivo de "entrar sem
mexer em tantos arquivos"; nada disso mudou desde a revisão anterior):

| Ponto | Hoje exige tocar | Proposta |
| --- | --- | --- |
| Gatilho novo (`look`, `talk`, `on_enter`) | enum `InteractionTriggerKind`, `InteractionTrigger::matches`/`wildcardLevel`, `if/else` em `InteractionFiles::parse` ([linhas 62-78](../src/content/InteractionFiles.cpp)) e em `save`, método novo em `Game` | `TriggerCatalog` no modelo do `ActionCatalog` (palavra-chave + assinatura tipada) |
| Campo novo de objeto (`name`, `approach`, `baseline`) | cadeia de `if` em `RoomFiles::load` ([linhas 147-162](../src/content/RoomFiles.cpp)), escrita manual em `RoomFiles::save`, `Room::validationError`, editor | `ObjectFieldCatalog` (chave, leitura, escrita, validação), como `RoomParameters` |
| Campo novo de clip (foi o caso do `flip_x`) | `SpriteClip`, cadeia de `if` em `SpriteFields::parse`, lista manual em `SpriteFields::write`, tabela do `GAME_DESIGN.md` | mesma tabela de campos, compartilhada com o catálogo de objeto |
| Comando novo do editor | ✅ resolvido no F2: uma entrada + uma função em `kCommands` ([debug/CommandCatalog.cpp](../src/debug/CommandCatalog.cpp)); `DebugEditor::submit` só repassa a linha ao `CommandConsole` | `CommandCatalog` (nome, uso, descrição, função) ✅; `help` e a lista do painel saem dele, e o `--exec` usa o mesmo catálogo |
| UI de gameplay nova (inventário, menu) | `Renderer::render(..., const DialogueMenu*)` ganha um parâmetro por UI, mais `Application` | registro de desenhistas de UI no `Renderer` (como `RenderOverlay`), ordenados por camada |
| Campo novo no save | `SessionState`, `SessionSnapshot` (+ `validationError`, `snapshot`, `restore`), `SaveFiles::parse` e `serialize` | tabela de campos do save (chave → ler/gravar) ou, no mínimo, um teste que falha quando um campo do snapshot não é serializado |
| Ator além do player | `Room` carrega `playerAnimations`, `playerBaseSize`, velocidades e `playerStart` | definição `Actor` própria (M7) |
| Jogo que precisa de uma ação própria | editar `ActionCatalog.cpp` da engine (vetor estático) | catálogos registráveis (L1) |

### 2.2 Multiplataforma

**Respeitada no código, não comprovada fora do macOS.** O que está no lugar: leitura por
`SDL_IOStream` em todos os formatos, nas fontes e nos PNGs (`SDL_LoadPNG`); caminhos UTF-8
(`u8string`) de ponta a ponta; escrita atômica em `SDL_GetPrefPath`; toque sem mouse sintético;
gamepad com hotplug, *deadzone* e cursor virtual; ciclo de vida num *event watch*; teclado
virtual só sob demanda; áudio fora do `SDL_Init` até existir módulo; arquivos opcionais
encontrados abrindo pelo SDL (APK). Viewport lógica fixa com letterbox e texto rasterizado na
densidade de pixels da saída.

Riscos concretos antes de um primeiro build mobile (inalterados):

- Não existe projeto Android/Xcode, nem `SDL_MAIN_USE_CALLBACKS` (o loop bloqueante de
  [Application.cpp:262](../src/core/Application.cpp) não serve a iOS/Web sem adaptação), nem
  *safe area*, nem orientação travada, nem botão voltar do Android.
- `writeTextAtomic` usa `std::ofstream` + `std::filesystem::rename`
  ([TextFormat.cpp:205](../src/content/TextFormat.cpp)); consoles pedem a API de armazenamento
  da plataforma (`SDL_Storage`) e nem todos têm `std::filesystem` completo. `std::filesystem::path`
  aparece nas interfaces de `content/` e até na validação de `game/SpriteClip.cpp`.
- A CI existe desde 11/11 (M17) mas ainda não rodou: Windows/MSVC nunca foi compilado; Linux só
  pela leitura do workflow. GCC 16 no macOS: sem avisos, com `-Werror` (repetido em 11/11).
- A entrada só tem ação primária: falta botão direito / toque longo / *release* (M3), que é o
  que substitui o *hover* no toque.
- O caso Android do C8 (`player.actor` dentro do APK) continua sem aparelho para conferir.

### 2.3 Eficiência

**Respeitada.** Medido em Release neste Mac (para comparar no futuro, não como requisito):

| Medida | 09/11 | 10/11 |
| --- | --- | --- |
| `Game::tick` com 300 objetos animados | 0,0002 ms | 0,0005 ms |
| Alocações de heap em `Game::tick` (181 ticks andando + 600 parado) | — | **0** |
| Pathfinding por clique, sala 1280×720 | média 0,03 ms, pior 0,09 ms | média 0,02 ms, pior 0,10 ms |
| Pathfinding, sala 4096×4096 com 104 obstáculos | média 0,9 ms, pior 1,6 ms | média 0,4 ms, pior 1,4 ms |
| Pathfinding, sala 8192×8192 com 419 obstáculos | média 3,0 ms, pior 7,3 ms | média 1,9 ms, pior 5,4 ms |
| Corredor em zigue-zague 8192×8192 (caso patológico) | 400 ms | 71 ms (outro traçado, 23 paredes; ainda 4 frames num clique) |
| `Scene::pick` com 300 objetos × 600 regras | 0,09 ms | 0,10 ms |

As salas das duas medições foram geradas por sondas diferentes; a ordem de grandeza é o que
vale. A simulação não é gargalo. No render, por leitura: buffer de ordenação reaproveitado
(`thread_local`), layouts de fala e de menu em cache por revisão, cache de `TTF_Text` por linha
com chave reaproveitada, cache de texturas por arquivo que sobrevive a edições e a ir e voltar
entre duas salas, validação de clip no carregamento e checagem O(1) no desenho, VSync ligado.

Riscos que continuam (todos já tinham item):

- **PNG carregado no primeiro desenho**, de forma síncrona
  ([SpriteLibrary.cpp:63](../src/graphics/SpriteLibrary.cpp)): engasgo no primeiro frame de cada
  sala quando houver arte de verdade (M9).
- **Suavização da rota é O(n²)** com `clearLine` sobre caixas grandes
  ([Navigation.cpp:200](../src/game/Navigation.cpp)) e o A* ainda chama `clearLine` por vizinho
  ([linha 176](../src/game/Navigation.cpp)); é o que explica o caso patológico (M14).
- **Picking** faz varredura dupla objetos × regras; vai rodar por tick quando houver *hover* (M3).
- Sem *culling*: objetos e as linhas-guia do chão são enviados ao SDL mesmo fora da câmera
  (irrelevante hoje; as guias somem com o M2).

---

## 3. O que já foi feito

### 3.1 Bugs, dívidas e features concluídos

Todos conferidos no código nesta revisão; "teste" é o teste de regressão que falha sem a correção.

| # | O quê | Como ficou | Teste |
| --- | --- | --- | --- |
| C1 | Softlock: `wait_animation` em clip com `loop = 0` e `fps = 0` | `AnimationPlayback::finished` trata `fps <= 0` como terminado | `GameTests.cpp` |
| C2 | Fala cortada, sem quebra de linha, com deriva | resolvido pelo M1: TTF, quebra por palavras e `\n`, centralizada acima do player | `TextLayoutTests.cpp`, `testSpeechUsesTtfText` |
| C3 | BOM UTF-8 quebrava todos os formatos | removido em `TextFormat::readFile`/`readOptionalFile` | `GameTests.cpp`, `EditorTests.cpp` |
| C5 | Fala atravessava a troca de sala | `speech_.hide()` em `Scene::apply` com `resetPlayer` | `GameTests.cpp` |
| C6 | Editor com painel oculto engolia setas/Enter | `DebugEditor::onToolAction` devolve `false` nesse caso | `EditorTests.cpp` |
| C7 | `load` do editor não recarregava PNGs | callback `setArtReloader`, comando `reload` | `EditorTests.cpp`, `SpriteTests.cpp` |
| C8 | Android: `player.actor` procurado com `std::filesystem::exists` | `ActorFiles::loadOptional` abre pelo SDL | desktop testado; Android não |
| C9 | Sem driver de áudio o jogo não abria | `SDL_Init(SDL_INIT_VIDEO)` até existir `audio/` | executável headless |
| C10 | Progresso perdido em crash | `AutosavePolicy::checkpoint` em `RoomEntered` | `GameTests.cpp` (a gravação em si fica em `Application`) |
| C11 | Player "ajeitava" os pés antes de interagir já ao alcance | `Scene::approach` resolve na hora abaixo de uma célula | `GameTests.cpp` |
| C12 | Uso de item sem regra não dava resposta | resolvido pelo F4 (regras curinga) | `testWildcardRules` |
| C13 | Overflow de contador: `add_flag` somava em `int` sem limite e o save aceitava qualquer `int` (`flag.x = 2147483647` + `add_flag x 1` era overflow com sinal) | `kMaxFlagValue` (±1.000.000.000) em `game/SessionState.hpp`; `incrementFlag` soma em 64 bits e satura, `setFlag` também não guarda valor fora do limite; `SessionSnapshot::validationError` recusa flag fora dele, o que cobre `SaveFiles::parse`/`save` e `SessionState::restore`. Sem chave nova: o save continua na versão 2 | `testFlagCounterLimits` em `GameTests.cpp` (saturação nos dois sentidos por `add_flag`, extremos de `int`, snapshot e `save.slot` recusados com `Flag fora do limite de +-1000000000: x = 2147483647`); suíte limpa sob UBSan |
| C4 | Estado visual de objeto não persistia: `play_animation porta open` voltava ao clip inicial ao sair e voltar da sala e ao carregar o save | Efeito novo `set_animation <obj> <clip>` (uma entrada no `ActionCatalog`, sem método novo no `ScriptHost`): grava o clip em `ObjectStateOverride::animation` (`SessionState::setObjectAnimation`/`objectAnimation`, `SessionSnapshot::ObjectOverride::animation`) e toca. `ObjectAnimations::rebuild(room, session, previous)` parte do clip salvo, já no último quadro se ele não repete (`AnimationPlayback::finish`). `play_animation` não mudou (continua do momento). Save: chave `object.<sala>.<objeto>.animation`, `SaveFiles::currentVersion` = 3, versões 1 e 2 continuam lidas; clip que deixou de existir é ignorado | `testObjectAnimationPersists` em `GameTests.cpp` (ida e volta entre salas, `play_animation` passageiro, edição ao vivo, round-trip determinístico do save v3, leitura de v2, chave recusada em v2, clip e objeto inexistentes, snapshot inválido); round-trip do catálogo em `EditorTests.cpp` |
| D1 | Desenho de debug em produção | rota, destino, painel técnico e dica em `debug/`, atrás de `DebugViews` (`show <visão> <0\|1>`) | `testGameFrameHasNoDebugDrawing`, `EditorTests.cpp`; o resto saiu no D10 |
| D2 | Sem VSync | `SDL_SetRenderVSync`, limitador como reserva, sem render com a janela oculta | executável headless |
| D3 | `Game::session()` mutável | só leitura; `Game::changeSession(função)` reconstrói a navegação | `testSessionCommandKeepsNavigation` |
| D4 | Documentação defasada | corrigida; relatórios antigos em `docs/historico/` | — (defasou de novo; ver D9) |
| D5 | Avisos estritos | `POINTCLICK_WARNINGS` em todos os targets; zero avisos no clang (repetido nesta revisão) e no GCC 16.2 (conferido antes, não repetido) | build |
| D6 | `Room.cpp` incluía `Player.hpp` | `game/PlayerClips.hpp` + regra em `CheckLayerIncludes.cmake` | `pointclick_layer_includes` |
| D7 | Gravação de sala ruidosa (`53.3333321`) | `TextFormat::number` (decimal mais curto que relê igual) | `GameTests.cpp`, `EditorTests.cpp` |
| D8 | Ponteiro preso fora da janela | `SDL_EVENT_WINDOW_MOUSE_LEAVE` | `EditorTests.cpp` |
| D9 | Documentação defasada (de novo) | `ARCHITECTURE.md`: gatilho com curingas, condições e efeitos por família com exemplos e ponteiro para a tabela do `GAME_DESIGN.md` e para `game/ActionCatalog.cpp` (o catálogo não é mais copiado), `ArgType` descrito por referência ao header, ordem das regras por especificidade (`wildcardLevel`, `findMatchingRule`); `README.md` apresenta a engine em vez de "protótipo" | — (documentação) |
| D10 | Base de colisão desenhada em produção | `EntityRenderer::drawObject` perdeu o preenchimento vermelho e o parâmetro `solidOverride`; o placeholder (retângulo na cor do objeto + contorno) fica até o M2, e a base só aparece no contorno do `DebugOverlay`, com o editor ativo | `testGameFrameHasNoDebugDrawing` (objeto sólido sem sprite: a cor da base não aparece no frame e o centro da base tem a cor do objeto) |
| D11 | `--no-save` ainda mexia no disco (save recusado virava `.bad`) | `SaveFiles::resume` recebe `RefusedSave` (`quarantine` ou `keep`), sem valor padrão; `Application::resumeSavedGame` passa `keep` com `--no-save` e usa o mesmo valor na quarentena que ele próprio faz; o log diz o que aconteceu com o arquivo | `testBadSaveFiles` em `GameTests.cpp` (os dois modos: arquivo intacto byte a byte e sem `.bad` com `keep`; renomeado com `quarantine`); executável headless nos dois modos |
| D12 | Erros de sala sem linha (`Room::validationError` só era conferido no fim de `RoomFiles::load`) | `Room::validationIssue` devolve `RoomValidationIssue` (mensagem, sujeito sala/spawn/objeto, posição, campo) e continua sendo a única fonte das regras; `validationError()` é a mensagem dela. `RoomFiles::load` guarda a linha de cada cabeçalho `[object]`/`[spawn]` e de cada chave e prefixa `Linha N:` com a chave que alimentou o erro (`bounds`, `collision`, `animation.initial`, `feet`) ou, sem ela, o cabeçalho; ID inválido ou repetido é recusado no próprio cabeçalho; `Spawn sem feet` também ganhou a linha. Textos iguais, sem chave nova. Erros da sala como um todo continuam sem linha | `testRoomObjectErrorsCarryLines` e os casos de `[spawn]` em `testRoomSpawnFilesAndTransitions` (`EditorTests.cpp`) |
| F1 | Opções `--new-game`, `--no-save`, `--room <id> [spawn]`, `--check` | `core/CommandLine` (sem SDL) + `content/ContentCheck` | `CommandLineTests.cpp`, `pointclick_check_*` |
| F2 | Sem comandos de sessão no editor: a regra `[use_item brass_key red_box]` dos assets não podia ser disparada no jogo (não há UI de inventário), e cada comando novo exigia mexer na cadeia de `if` de `DebugEditor::submit` e no texto fixo de `DebugOverlay.cpp` | **`CommandCatalog`** (`debug/CommandCatalog.*`, 2.1): `CommandDescriptor{nome, uso, descrição, função}`; os 12 comandos antigos migraram sem mudar comportamento nem texto de erro (o "Use: show route\|diagnostics\|hint 0/1" agora sai de `kViewSwitches`); `help` (lista + uma linha "uso -- descrição" por comando), `help <comando>` e a lista do painel saem da tabela. **`CommandConsole`** (`debug/CommandConsole.*`) guarda o estado das ferramentas (biblioteca, recarga de arte, `undo`/`reset`, `DebugViews`, sala do `newgame`) e executa uma linha → `CommandResult{ok, output}`, sem UI; o `DebugEditor` virou só entrada e hit-test. **Comandos de sessão**, todos por `Game::changeSession` ou por intenção do `Game`: `flag <id> <n>` (recusa fora de ±1.000.000.000, sem saturar em silêncio; `0` apaga), `give`/`take <item>`, `showobj`/`hideobj <sala.objeto\|objeto>` (nomes espelham `show_object`/`hide_object`, porque `show` já é das visões; outra sala é conferida no arquivo), `use <item> <obj>` (`Game::useItemOnObject`, curingas incluídos), `goto <sala> [spawn]` (`Game::requestRoomChange` → `RoomChangeRequested`, o caminho do `change_room`), `newgame` (`Game::startNewGame`: cancela sequência e diálogo, zera a sessão e pede a sala inicial, `first` ou a do `--room`), `state` (sala, flags e overrides ordenados, itens na ordem do inventário, escolhas de diálogo). **Headless:** `--exec "<comando>"` e `--exec-file <arquivo>` (repetíveis, em ordem; parsing em `core/CommandLine`, só com `POINTCLICK_TOOLS`) rodam em `debug/CommandScript` um jogo novo que nunca lê nem grava o save, assentam a simulação a 60 Hz após cada comando, imprimem `> comando`, resposta (`erro: ` nas falhas), `fala:` e `sala:`, e saem com 1 se algum comando falhar. A carga de sala de um `RoomChangeRequested` virou `enterRoom` (`content/RoomLibrary`), usada pelo `Application` e pelo headless. Formatos e save (versão 3) não mudaram | `testCommandCatalogHelp`, `testPanelTextRows`, `testFlagCommand`, `testItemCommands`, `testObjectVisibilityCommands`, `testUseCommand`, `testGotoCommandKeepsSession`, `testGotoCommandSpawn`, `testNewGameCommand`, `testStateCommand` (`EditorTests.cpp`); `testNewGameAndRoomRequests` (`GameTests.cpp`, roda também na loja); `testParseExecOptions`, `testExecUsesBrassKeyOnShippedRedBox`, `testExecReportsFailures`, `testExecFile` (`CommandLineTests.cpp`); `pointclick_exec_assets`, `pointclick_exec_assets_report` e `pointclick_exec_failure` no `ctest`; binário da loja sem nenhum símbolo de `debug/` e recusando `--exec` |
| F4 | Regras curinga (`[interact *]`, `[use_item * obj]`, `[use_item item *]`, `[use_item * *]`) | ordem fixa por especificidade em `findMatchingRule` | `testWildcardRules`, round-trip em `EditorTests.cpp` |
| F5 | Contadores: `add_flag`, `flag_at_least`, `flag_equals` | três entradas no catálogo + `ArgType::integer` | `testFlagCounters`; limite no C13 |
| F6 | `face <direção>` | `ScriptHost::facePlayer` + `ArgType::facing` | `testFaceDirection` |
| F7 | `flip_x` nos clips | campo em `SpriteClip`/`SpriteFields`, XOR com o chamador | pixels em `SpriteTests.cpp`, round-trip |
| F8 | Arte ausente só aparecia em jogo (quadriculado roxo, fonte de debug) | Continua não sendo erro: a sala carrega e o jogo roda. `clipFitsImage`/`spriteSheetColumns` viraram funções puras em `game/SpriteClip` (o `SpriteLibrary` usa as mesmas). `content/ArtCheck` lê só o cabeçalho do PNG por `SDL_IOStream` (cache por arquivo na chamada) e devolve avisos: PNG ausente ou ilegível, corte que não cabe, clip sem arquivo, fonte ausente (lista vinda de `FontLibrary::files`, injetada). `Application::logArtWarnings` registra cada aviso uma vez por execução ao carregar a sala; `--check` imprime `aviso: <sala>: <arquivo>: <motivo>`, conta no resumo e **não** muda o código de saída | `testArtWarnings` e `testRoomWithMissingArtStillRuns` (`SpriteTests.cpp`, frame headless com o quadriculado), `testCheckWarnsAboutMissingArt` (`CommandLineTests.cpp`), `pointclick_check_missing_art` (sai 0) e `pointclick_check_missing_art_report` no `ctest`, fixture `tests/fixtures/missing_art` |
| M17 | Qualidade (parcial): testes sem nome de caso, sem CI, fuzz só à mão | **doctest** 2.4.12 por CPM com hash fixo (`POINTCLICK_DOCTEST_VERSION`, só com `BUILD_TESTING`); os cinco executáveis viraram 62 `TEST_CASE` com os nomes das funções e 1.319 `REQUIRE` (mesma contagem dos `assert`), `main` único em `pointclick_test_main`, mesmo link por camada, um `add_test` por executável, filtros do doctest. Cada caso cria a própria pasta (`tests/support/TempDirectory.hpp`); `SpriteTests` tinha dois casos que dependiam do `sheet.png` deixado por outro (agora `writeSheetPng`). **CI** em `.github/workflows/ci.yml`: macOS (AppleClang), Linux (clang, gcc), Windows (MSVC, gerador padrão), Linux ASan + UBSan (preset `asan`) e Linux Release sem ferramentas (preset `store`); cada job roda os presets, o `ctest` e `pointclick --rooms assets/rooms --check`, com cache do CPM e ccache; `POINTCLICK_WARNINGS_AS_ERRORS` ligado fora do Windows. **Fuzz**: `pointclick_parser_fuzz` (`POINTCLICK_ENABLE_FUZZ`, fora do `ctest`), laço determinístico que muta os assets e sementes com inteiros com sinal, `face`, curingas, `flip_x`, `set_animation` e o save v3. `src/` não mudou | Falha proposital em `testFlagCounters` relatada com caso, linha e valores (`REQUIRE( 0 == 42 )`); cada um dos 62 casos passa sozinho e em ordem aleatória; `ctest` 10/10 em debug, ASan + UBSan e GCC 16 com `-Werror`, 9/9 na loja (clang e GCC), também com SDL3/SDL3_ttf compilados pelo CPM; fuzz com 360.018 entradas (sementes 20261111 e 7) sem crash nem relatório. **Não provado:** a execução no GitHub Actions (3.3) |
| M1 | Texto de verdade (parcial) | SDL3_ttf, `FontLibrary`, `TextRenderer`, `ui/TextLayout`, `ui/SpeechLayout`, `DialogueMenu` medindo o texto real | `TextLayoutTests.cpp`, `SpriteTests.cpp`; resto na seção 5 |

### 3.2 Estado dos testes

Cerca de 1.500 `REQUIRE` em 76 `TEST_CASE` do doctest, cinco executáveis (`pointclick_tests` 30
casos, `pointclick_sprite_tests` 8, `pointclick_text_tests` 7, `pointclick_cli_tests` 8,
`pointclick_editor_tests` 23), mais oito testes de linha de comando/camadas no `ctest` (os três
`pointclick_exec_*` só com ferramentas). Uma falha nomeia o caso, o arquivo e a linha e os outros
casos continuam; cada caso roda sozinho (`--test-case=`). Na contagem de 11/11 (1.319 verificações,
antes do F2), 234 tinham `&&`/`||` e ficam entre parênteses duplos: o relatório delas mostra a
expressão, não os valores dos dois lados. Os laços de pixels dos testes gráficos passam por
`REQUIRE` (cerca de 21 milhões de verificações por execução), o que levou o `ctest` de debug de
~0,8 s para ~3 s (15 s sob ASan). Nenhum teste **joga** os assets reais: `pointclick_check_assets`
só prova que eles carregam (F3).

### 3.3 Verificações em aberto dos itens concluídos

Tudo aqui depende de uma janela com GPU, de outra plataforma ou de um asset que ainda não existe:

- **VSync** (D2) e **`flip_x`** (F7) numa janela com GPU real.
- **Android** (C8): leitura do `player.actor`, das fontes e dos PNGs de dentro do APK.
- **CI no GitHub Actions** (M17): o workflow nunca rodou. Foi validado só como YAML bem formado
  e pela execução local, no macOS, dos mesmos comandos (presets `debug`, `asan`, `store`, clang e
  GCC 16, com e sem o SDL do sistema). Pontos de risco até a primeira execução real:
  - **Windows/MSVC** (D5): `/W4 /permissive-` nunca foi compilado, nem o doctest e o SDL3 +
    SDL3_ttf + FreeType do CPM com o gerador do Visual Studio; `/WX` está desligado nesse job
    até se saber o que o `/W4` diz. O FreeType vem de `download.savannah.gnu.org`, que costuma
    oscilar. O compilador de cache não vale com o gerador do Visual Studio (só o cache do CPM).
  - **Linux**: GCC 13 e clang 18 do `ubuntu-24.04` (aqui só GCC 16 e AppleClang) com `-Werror`;
    a lista de pacotes `-dev` para o SDL3 do CPM; os testes gráficos sem display (driver
    `dummy` + renderer de software, provado só no macOS); LeakSanitizer, que no Linux vem ligado
    com o ASan e no macOS não existe.
  - **macOS do runner**: versão do SDL3/SDL3_ttf do Homebrew no dia (se for anterior à de
    referência, o CPM compila o SDL3 e o SDL3_ttf do Homebrew ficaria ligado a outro SDL3).
  - Chaves de cache, `concurrency` e o tempo dos jobs (SDL3 compilado do zero no primeiro run).
- **`--room <sala> <spawn>`** (F1) com um spawn válido: os assets não têm `[spawn]`.
- **Autosave de saída com `--no-save`** (F1): o SIGINT não encerra o jogo com o driver `dummy`.
- **Avisos de arte** (F8) no Android: a leitura do cabeçalho do PNG usa `SDL_IOStream`, mas só
  foi exercitada no desktop; o log de `Application::logArtWarnings` não tem teste (só a função
  de `content/` que ele chama).
- **`set_animation`** (C4) numa janela real e num playground com porta: os assets ainda não têm
  objeto com clip (F3).
- **Fuzz dos parsers** (M17): roda à mão no macOS; não está na CI nem em outro compilador.
- **Comandos de sessão** (F2): conferidos numa janela real (macOS, Metal) em 13/11: painel com
  `help` e `state`, `goto gallery` e `newgame` passando por `Application::processGameEvents`
  (sala trocada, progresso mantido / zerado, `red_box` sólida de novo) e `use brass_key red_box`
  rodando a regra pelo loop do jogo. O teste achou e corrigiu o status cortado em silêncio
  (`wrapPanelText`, `testPanelTextRows`). `newgame` com o autosave ligado também foi conferido
  (save real copiado antes e restaurado byte a byte depois): retomando uma partida na `gallery`, o
  `newgame` regravou o `save.slot` no `RoomEntered` com `room = first`, os pés no início e nenhuma
  flag ou item. Falta: o `undo`/`reset` depois de um `goto` (continuam apontando para a sala de quando o editor
  abriu, como depois de um `load`). O relatório do `--exec` sai em UTF-8 (falas com acento): no
  console do Windows depende da página de código.

---

## 4. Curto prazo (horas a dias)

### 4.1 Bugs confirmados

Nenhum em aberto (C4 foi concluído; seção 3.1).

### 4.2 Dívidas e higiene

Nenhuma em aberto (D12 foi concluído; seção 3.1).

### 4.3 Features pequenas de alto valor

- **F3 — Playgrounds por mecânica + teste de percurso:** os assets atuais não exercitam em jogo
  real `change_room`, `[spawn]`, câmera (as duas salas têm 1280 de largura), sprites/animação de
  objeto, `walk_to`, `wait`, `wait_animation`, nem nada do que entrou depois: regras curinga,
  contadores, `face` e `flip_x`. Criar salas pequenas (seção 8) e um teste headless que carrega os
  assets e joga cada sala com uma lista de cliques até um estado esperado (o `--exec` do F2 já
  monta estados e confere com `state`; falta um comando de clique/interação para jogar o percurso).
  Fecha também metade da lista 3.3. *2–3 dias.*

### 4.4 Ordem sugerida

1. Empurrar o M17 e olhar a primeira execução da CI: é o que fecha (ou abre) os riscos de
   Windows/MSVC e Linux da lista 3.3. Custa minutos e vale para tudo o que vier depois.
2. ~~F2 com o `CommandCatalog`~~ ✅ (seção 3.1).
3. F3, para as mecânicas prontas terem prova em jogo real; o teste de percurso entra na CI sem
   trabalho extra, porque ela roda o `ctest` (F8 já avisa da arte ausente e C4 já dá à sala
   `door` o estado persistente, com o save na versão 3).
4. Daí em diante, M2 e M3 (seção 5).

---

## 5. Médio prazo (semanas)

Ordem sugerida: M2 primeiro (sem fundo, nenhuma outra mecânica é avaliável na tela), depois
M3–M6 (o núcleo de um point & click), depois o resto.

- **M1 — Texto de verdade: o que falta.** (O grosso está feito, seção 3.1.) **Orador:**
  `FontRole::speaker` e `SpeechLayout` já desenham o nome acima da fala, mas
  `FloatingText::speaker()` está sempre vazio: falta `say <texto> [orador]` (ou
  `say_as <ator> <texto>`) no `ActionCatalog`, `ScriptHost::say` com orador e
  `LineSpoken{speaker, text}`. É mudança de formato do `.rules`/`.dlg` e casa melhor com M7
  (atores/NPCs), que dá o ID do falante e a âncora da fala sobre ele em vez do player.
  **Menu longo:** um menu de diálogo com opções suficientes para passar da altura da tela não
  rola; o painel para na margem de cima e as últimas linhas saem da tela
  ([DialogueMenu.cpp:36](../src/ui/DialogueMenu.cpp)).
- **M2 — Fundo e camadas.** Hoje a sala é só cor chapada + guias, e objeto sem clip desenha um
  retângulo colorido: não dá para ter *hotspot* invisível sobre uma arte de fundo. Acrescentar
  `[layer <id>]` (arquivo, `scroll_factor` para parallax, atrás/à frente, `baseline`),
  `[hotspot <id>]` invisível e `baseline` por objeto, com picking e render usando a mesma função
  de profundidade. Toca: `Room`, `RoomFiles` (via `ObjectFieldCatalog`), renderer de camadas
  novo em `graphics/`, `Interaction.cpp`.
- **M3 — Verbos, hover e cursor.** `TriggerCatalog` (`look`, `talk`, `pick_up`...); `input/` com
  ação secundária (botão direito, toque longo) e *release*; `Scene::hoveredObject()` recalculado
  por tick a partir do ponto na viewport (hoje `Scene::onPointerMove` guarda o ponto no mundo,
  que fica velho quando a câmera rola); `name`, `approach = x, y` e `facing` por objeto; rótulo
  de hover e cursor em `ui/`. Montar o índice `objeto → regras` no commit da sala para o hover
  custar O(objetos). O painel do editor também precisa bloquear o *hover* do mundo: hoje o
  `InputRouter` só barra o clique.
- **M4 — Inventário.** Catálogo `assets/items.items` (nome, ícone, descrição), `ui/InventoryPanel`
  como `ViewportUiConsumer`, item selecionado no cursor, gatilho `[combine <a> <b>]`, eventos
  `ItemAdded`/`ItemRemoved`. A API do domínio (`useItemOnObject`, `useItemAt`) já existe.
  Depende do registro de desenhistas de UI (2.1).
- **M5 — Gatilhos de sala.** `[on_enter]`, `[on_first_enter]`, `[on_exit]`, zonas
  (`[zone <id>]` + `[enter_zone <id>]`) e temporizadores, todos no mesmo `ScriptRunner`. Com
  scripts que rodam enquanto o player anda, passa a valer **revalidar a rota em curso** quando a
  colisão muda (hoje `Scene::refreshNavigation` só reposiciona quem ficou dentro de um
  obstáculo, e `Game::changeSession` tem a mesma lacuna).
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
  volumes nas configurações. Target novo `pointclick_audio`, dependendo só de `game`; o
  subsistema de áudio é iniciado à parte e o jogo segue sem som se falhar (C9).
- **M9 — Transições.** *Fade* como estado temporizado em `graphics/`; a troca de sala passa a ter
  duas fases (escurece → carrega → clareia) com a entrada bloqueada; pré-carregar as texturas da
  sala em `RoomEntered`, durante o escuro.
- **M10 — Manifesto e configuração do app.** `assets/game.manifest` (título, organização e nome
  para a pasta de saves, sala inicial, lista de salas, idiomas). Hoje estão fixos no código
  `first.room` (`RoomLibrary::startRoomFile`), "Point & Click"
  ([Application.cpp:206](../src/core/Application.cpp)) e `SaveFiles::organization/application`.
  Validação cruzada de tudo no boot em builds de desenvolvimento (e no `--check`); o manifesto
  também tira do `ContentCheck` a listagem da pasta com `std::filesystem`.
- **M11 — Entrada.** Tabela de *bindings* remapeável (hoje são dois `switch` em
  `InputSystem::translateEvent`), repetição no D-pad, ciclo entre hotspots com *snap* no
  gamepad.
- **M12 — Menus e configurações.** Pausa, slots de save com salvar/carregar manual, arquivo de
  configurações no *pref path* (volume, tela cheia, idioma, velocidade do texto), alternar tela
  cheia. Depende do registro de UIs (2.1).
- **M13 — Localização.** `@chave` em `say` e nos textos de escolha, `text/<idioma>.strings`,
  chaves ausentes acusadas no carregamento.
- **M14 — Navegação.** Limitar a suavização (parar no primeiro candidato bloqueado ou limitar o
  alcance) e usar só a regra de quina dentro do A*/BFS; ponto caminhável mais próximo
  considerando alcance (`Scene::findNearestWalkable` varre a grade inteira); área caminhável
  por polígonos ou máscara PNG rasterizada na mesma grade.
- **M15 — Save.** Backup `save.slot.bak` antes de substituir, metadados (data, tempo de jogo,
  sala), slots; manter a regra "chave nova = versão + 1".
- **M16 — Editor.** Arrastar objetos com o mouse; criar/remover objetos, spawns e hotspots;
  overlays por flag (grade de navegação, hotspots, pontos de aproximação; `DebugViews` já é o
  lugar); recarga automática de `.rules`/`.dlg` ao salvar o arquivo; histórico de comandos; o
  `save` do editor descarta os comentários do designer no `.room`.
- **M17 — Qualidade (o que sobrou).** doctest, CI, presets `asan`/`store` e o fuzz como target
  estão prontos (seção 3.1). Falta: (1) **rodar a CI de verdade** e corrigir o que ela mostrar
  (riscos na seção 3.3), depois ligar `/WX` no job do Windows; (2) teste de **frame de
  referência** (comparar um frame headless com uma imagem guardada); (3) fuzz e `format-check`
  como jobs da CI; (4) cache de compilação no Windows (Ninja + MSVC ou sccache); (5) trocar por
  comparações simples as 234 verificações compostas, para o relatório mostrar os valores, e
  tirar os laços de pixels do `REQUIRE` se o tempo sob ASan incomodar.

---

## 6. Longo prazo (meses)

- **L1 — Engine como base reutilizável.** É o que o objetivo do projeto pede e hoje não existe:
  a engine é um executável + uma pasta de assets. Passos: separar `engine/` de `playground/`
  (assets e salas de teste); exportar targets CMake (`pointclick::engine`) para um jogo consumir
  por `FetchContent`/`add_subdirectory`; **catálogos registráveis** (ações, condições, gatilhos,
  comandos do editor) para o jogo acrescentar vocabulário sem editar a engine; `Application`
  configurável (`AppConfig` + pontos de registro de UI, overlays e consumidores de evento);
  projeto-modelo de jogo; versionamento dos formatos de conteúdo; guia "como criar um jogo".
  Jogos só de dados continuam possíveis apontando o binário para outra pasta de assets.
- **L2 — Plataformas.** `SDL_MAIN_USE_CALLBACKS` (`Application` já separa `initialize`,
  `processEvents` e `shutdown`; falta extrair uma iteração do `mainLoop`); projeto Android
  (assets no APK, orientação, botão voltar, ciclo de vida em aparelho); projeto iOS
  (*safe area*); Web/Emscripten (saves em IDBFS); consoles. Matriz de aparelhos de teste.
- **L3 — Abstração de I/O.** `content/AssetLocator` + `SDL_Storage` (título para leitura,
  usuário para escrita) no lugar de `std::filesystem` e `std::ofstream`; pacotes de assets;
  carregamento assíncrono com tela de carregamento.
- **L4 — Render.** Atlas de texturas, *render targets* para transições e efeitos, tinta/luz
  ambiente por região, partículas simples, política de resolução (escala inteira opcional para
  pixel art, assets em mais de uma densidade), *culling* pela câmera.
- **L5 — Ferramentas.** Editor de salas mais completo ou pipeline de importação (Tiled,
  Aseprite); depurador de scripts (passo a passo, inspeção de flags); visualização do grafo de
  diálogo; gravação e reprodução determinística de entrada para regressão.
- **L6 — Scripting avançado.** Variáveis, expressões e blocos `if/else`; decidir se a lógica de
  jogo mais complexa fica declarativa ou vai para uma linguagem embutida atrás do `ScriptHost`.
  Adiar enquanto o catálogo der conta.
- **L7 — Recursos narrativos da engine.** Voz por linha, log de falas, legendas configuráveis,
  conquistas e telemetria como consumidores de `GameEvent` (`LineSpoken` e
  `CutsceneStarted`/`Ended` já são emitidos e ainda não têm consumidor).
- **L8 — Acessibilidade.** Escala de texto, alto contraste, destacar todos os hotspots,
  remapeamento completo.
- **L9 — Orçamento de desempenho.** Perfil em hardware fraco (tempo de boot, memória de
  texturas, bateria), render sob demanda quando nada muda, instrumentação opcional; a sonda
  desta revisão (tempos e contagem de alocações) como target de benchmark.
- **L10 — Navegação avançada.** Polígonos com *funnel*, regiões com escala própria, vários
  andares, obstáculos móveis (NPCs), máscaras de *walk-behind*.

---

## 7. Dependências entre os itens

```text
M1 texto ✅ ────────────────┬─► M3 hover/verbos ─► M4 inventário ─► M12 menus
D1 flags de debug ✅ ───────┤                 └─► M13 localização
M2 fundo/camadas/hotspots ──┘
D3 ✅ + F2 ✅ (comandos de sessão, --exec) ─► F3 playgrounds + teste de percurso ─► CI (M17 ✅ parcial: falta rodar)
F8 (aviso de arte no --check) ✅ ───────────────────────────────────────┘
C4 (estado persistente de objeto) ✅ ─► M15 save
C13 (limite dos contadores) ✅ ─► add_flag em laço já não estoura o contador
M17 doctest + CI + fuzz ✅ (parcial) ─► primeira execução no GitHub Actions ─► MSVC/Linux da 3.3
M5 gatilhos ─► M6 scripts ─► M7 atores ─► orador do M1
M10 manifesto ─► L1 engine reutilizável ─► L2 plataformas ─► L3 I/O
```

## 8. Playgrounds sugeridos

Uma sala pequena por mecânica, com as regras mínimas, coberta pelo teste de percurso (F3):

| Sala | Exercita |
| --- | --- |
| `first` ↔ `gallery` ligadas por uma porta | `change_room`, `[spawn]`, `facing`, autosave em `RoomEntered`, `--room <sala> <spawn>` |
| `wide` (2560 de largura) | câmera suave e *clamp*, clique convertido com a câmera deslocada |
| `door` | objeto com sprites, `play_animation`, `wait_animation`, `flip_x`, estado persistente com `set_animation` (C4 ✅) |
| `lever` | sequência longa: `walk_to`, `face`, `wait`, `say`, modo cutscene, pular fala |
| `maze` | obstáculos densos, destino inalcançável, custo de rota (M14) |
| `talk` | diálogo com `once`, `goto`, condições, `change_room` dentro da conversa |
| `items` | pegar, usar item em objeto (`use` do F2 ✅ até existir inventário), regra curinga (F4), contadores (F5, C13) |
