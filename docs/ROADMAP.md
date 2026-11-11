# Revisão geral e roadmap da engine

**Data:** 10/11/2026 · **Commit revisado:** `65bf916` · **Escopo:** todo `src/`, `cmake/`, `assets/`,
`docs/` e a estrutura dos testes. Substitui a revisão de 09/11/2026 (commit `bd26b9e`); os
identificadores dos itens (C, D, F, M, L) foram mantidos, porque o código e os outros documentos
os citam.

**Atualização de 11/11/2026:** C13, D9, D10 e D11 foram concluídos (mudanças feitas sobre
`65bf916`) e passaram para a seção 3.1. O resto do documento continua sendo a revisão de 10/11.

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

**Desvios encontrados** (pequenos, viraram itens). Dois já foram corrigidos (seção 3.1): a
base de colisão vermelha que `EntityRenderer::drawObject` pintava também na build de loja (D10)
e o `ARCHITECTURE.md` atrás do catálogo e da ordem das regras curinga (D9). Resta um, na seção
4.2:

- Os erros de `Room::validationError` chegam ao designer sem número de linha, ao contrário de
  `.rules` e `.dlg` (item D12).

**Onde uma feature nova ainda espalha edições** (o que mais importa para o objetivo de "entrar sem
mexer em tantos arquivos"; nada disso mudou desde a revisão anterior):

| Ponto | Hoje exige tocar | Proposta |
| --- | --- | --- |
| Gatilho novo (`look`, `talk`, `on_enter`) | enum `InteractionTriggerKind`, `InteractionTrigger::matches`/`wildcardLevel`, `if/else` em `InteractionFiles::parse` ([linhas 62-78](../src/content/InteractionFiles.cpp)) e em `save`, método novo em `Game` | `TriggerCatalog` no modelo do `ActionCatalog` (palavra-chave + assinatura tipada) |
| Campo novo de objeto (`name`, `approach`, `baseline`) | cadeia de `if` em `RoomFiles::load` ([linhas 135-151](../src/content/RoomFiles.cpp)), escrita manual em `RoomFiles::save`, `Room::validationError`, editor | `ObjectFieldCatalog` (chave, leitura, escrita, validação), como `RoomParameters` |
| Campo novo de clip (foi o caso do `flip_x`) | `SpriteClip`, cadeia de `if` em `SpriteFields::parse`, lista manual em `SpriteFields::write`, tabela do `GAME_DESIGN.md` | mesma tabela de campos, compartilhada com o catálogo de objeto |
| Comando novo do editor | cadeia de `if` em `DebugEditor::submit` ([linhas 150-244](../src/debug/DebugEditor.cpp)) + texto de ajuda fixo em `DebugOverlay.cpp` | `CommandCatalog` (nome, uso, função); `help` e a ajuda do painel saem dele |
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
- Sem CI: Windows/MSVC nunca foi compilado; Linux/GCC só à mão, numa revisão anterior
  (GCC 16.2, sem avisos; não repetido agora).
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
| F1 | Opções `--new-game`, `--no-save`, `--room <id> [spawn]`, `--check` | `core/CommandLine` (sem SDL) + `content/ContentCheck` | `CommandLineTests.cpp`, `pointclick_check_*` |
| F4 | Regras curinga (`[interact *]`, `[use_item * obj]`, `[use_item item *]`, `[use_item * *]`) | ordem fixa por especificidade em `findMatchingRule` | `testWildcardRules`, round-trip em `EditorTests.cpp` |
| F5 | Contadores: `add_flag`, `flag_at_least`, `flag_equals` | três entradas no catálogo + `ArgType::integer` | `testFlagCounters`; limite no C13 |
| F6 | `face <direção>` | `ScriptHost::facePlayer` + `ArgType::facing` | `testFaceDirection` |
| F7 | `flip_x` nos clips | campo em `SpriteClip`/`SpriteFields`, XOR com o chamador | pixels em `SpriteTests.cpp`, round-trip |
| M1 | Texto de verdade (parcial) | SDL3_ttf, `FontLibrary`, `TextRenderer`, `ui/TextLayout`, `ui/SpeechLayout`, `DialogueMenu` medindo o texto real | `TextLayoutTests.cpp`, `SpriteTests.cpp`; resto na seção 5 |

### 3.2 Estado dos testes

1.229 `assert` em 57 funções de teste, cinco executáveis (`pointclick_tests`,
`pointclick_sprite_tests`, `pointclick_text_tests`, `pointclick_cli_tests`,
`pointclick_editor_tests`), mais três testes de linha de comando/camadas no `ctest`. A primeira
falha ainda aborta o executável sem nomear o caso (M17). Nenhum teste **joga** os assets reais:
`pointclick_check_assets` só prova que eles carregam (F3).

### 3.3 Verificações em aberto dos itens concluídos

Tudo aqui depende de uma janela com GPU, de outra plataforma ou de um asset que ainda não existe:

- **VSync** (D2) e **`flip_x`** (F7) numa janela com GPU real.
- **Android** (C8): leitura do `player.actor`, das fontes e dos PNGs de dentro do APK.
- **MSVC** (D5): `/W4 /permissive-` nunca foi compilado.
- **`--room <sala> <spawn>`** (F1) com um spawn válido: os assets não têm `[spawn]`.
- **Autosave de saída com `--no-save`** (F1): o SIGINT não encerra o jogo com o driver `dummy`.
- **Fuzz dos parsers** com o vocabulário novo (inteiros com sinal, direções, curingas, `flip_x`).

---

## 4. Curto prazo (horas a dias)

### 4.1 Bugs confirmados

| # | Problema | Evidência | Onde / correção | Esforço |
| --- | --- | --- | --- | --- |
| C4 | **Estado visual de objeto não persiste:** `play_animation porta open` volta ao clip inicial ao sair e voltar da sala e ao carregar o save, enquanto flag e colisão continuam "aberta". | Sonda de 09/11: `anim=idle`, `flag=1`, `solid=0`; `ObjectStateOverride` continua só com `visible` e `solidOverride`. | Campo `animation` em `ObjectStateOverride` + efeito persistente (`set_animation <obj> <clip>`, ou `play_animation` de clip sem loop passa a persistir); `ObjectAnimations::rebuild` parte do clip salvo já no último quadro. Toca `SessionState`, `SaveFiles` (versão 3), `ActionCatalog`, `Scene::apply`, docs, testes. | 1–2 dias |

### 4.2 Dívidas e higiene

- **D12 — Erros de sala sem linha.** O que `Room::validationError` acusa (objeto fora da sala,
  ID repetido ou inválido em `[object]`/`[spawn]`, animação inicial ausente, base de colisão)
  chega sem número de linha, porque só é conferido no fim de `RoomFiles::load`; `.rules` e
  `.dlg` sempre dão a linha. Validar o ID no cabeçalho e guardar a linha de cada seção para
  prefixar o erro. Sai de graça se for feito junto com o `ObjectFieldCatalog`. *1–2 h.*

### 4.3 Features pequenas de alto valor

- **F2 — Comandos de sessão no editor:** `flag <id> <n>`, `give`/`take <item>`,
  `show`/`hide <obj>`, `use <item> <obj>`, `goto <sala> [spawn]`, `newgame`, `state` (lista
  flags, itens e overrides). Sem isso a regra `[use_item brass_key red_box]` dos assets **não
  pode ser disparada no jogo**, porque não existe UI de inventário. `Game::changeSession` (D3)
  já é a porta de entrada. Fazer junto com o `CommandCatalog` (2.1) para o `help` sair da
  tabela. Atenção ao nome: `show` já é o comando das visões de debug. *1–2 dias.*
- **F3 — Playgrounds por mecânica + teste de percurso:** os assets atuais não exercitam em jogo
  real `change_room`, `[spawn]`, câmera (as duas salas têm 1280 de largura), sprites/animação
  de objeto, `walk_to`, `wait`, `wait_animation`, nem nada do que entrou depois: regras curinga,
  contadores, `face` e `flip_x`. Criar salas pequenas (seção 8) e um teste headless que carrega
  os assets e joga cada sala com uma lista de cliques até um estado esperado. Fecha também
  metade da lista 3.3. *2–3 dias.*
- **F8 — `--check` confere a arte (novo):** hoje PNG ausente, corte que não cabe na imagem e
  fonte faltando só aparecem em jogo (quadriculado roxo, fonte de debug); o `--check` passa.
  Mover `clipFitsImage` de `SpriteLibrary.cpp` para `game/SpriteClip` (função pura, o renderer
  continua usando) e fazer o `ContentCheck` abrir cada PNG referenciado com `SDL_LoadPNG` (sem
  janela) e conferir as fontes de `FontLibrary::spec`. Toca `SpriteClip`, `SpriteLibrary.cpp`,
  `ContentCheck.cpp`, `tests/CommandLineTests.cpp`, uma fixture. *½ dia.*

### 4.4 Ordem sugerida

1. F2 com o `CommandCatalog`, porque destrava testar à mão o que já existe.
2. F3 (com F8 no caminho), para as mecânicas prontas terem prova em jogo real e CI.
3. C4, que muda o save (versão 3) e é pré-requisito de qualquer porta com animação.
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
- **M17 — Qualidade.** Migrar para doctest (1.229 `assert` em cinco executáveis; a primeira
  falha aborta sem nomear o caso); **CI** em macOS/Linux/Windows com ASan/UBSan (não existe
  nenhuma: é o que transformaria "multiplataforma" de leitura em prova, e fecha o MSVC da lista
  3.3); o fuzz como target; teste de frame de referência; `--check` dos assets na CI.

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
D3 ✅ + F2 (comandos de sessão) ─► F3 playgrounds + teste de percurso ─► M17 CI
F8 (--check da arte) ───────────────────────────────────────────────────┘
C4 (estado persistente de objeto) ─► M15 save
C13 (limite dos contadores) ✅ ─► add_flag em laço já não estoura o contador
M5 gatilhos ─► M6 scripts ─► M7 atores ─► orador do M1
M10 manifesto ─► L1 engine reutilizável ─► L2 plataformas ─► L3 I/O
```

## 8. Playgrounds sugeridos

Uma sala pequena por mecânica, com as regras mínimas, coberta pelo teste de percurso (F3):

| Sala | Exercita |
| --- | --- |
| `first` ↔ `gallery` ligadas por uma porta | `change_room`, `[spawn]`, `facing`, autosave em `RoomEntered`, `--room <sala> <spawn>` |
| `wide` (2560 de largura) | câmera suave e *clamp*, clique convertido com a câmera deslocada |
| `door` | objeto com sprites, `play_animation`, `wait_animation`, `flip_x`, estado persistente (C4) |
| `lever` | sequência longa: `walk_to`, `face`, `wait`, `say`, modo cutscene, pular fala |
| `maze` | obstáculos densos, destino inalcançável, custo de rota (M14) |
| `talk` | diálogo com `once`, `goto`, condições, `change_room` dentro da conversa |
| `items` | pegar, usar item em objeto (F2 até existir inventário), regra curinga (F4), contadores (F5, C13) |
