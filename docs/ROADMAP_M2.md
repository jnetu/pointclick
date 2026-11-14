# M2 — Fundo, camadas e hotspots: plano de execução

**Data:** 14/11/2026 · **Origem:** `docs/ROADMAP.md`, seção 5 (M2) · **Estado:** em andamento (M2-01 feita) ·
**Tamanho:** 38 micro-tarefas, cerca de 6 a 9 dias de trabalho (S ≤ 1 h, M ≤ meio dia, L ≤ 1 dia).

Este documento fatia o M2 em passos pequenos, cada um com **o que toca**, **quando está pronto** e
**qual teste prova**. A ordem foi pensada para aparecer arte na tela cedo (marco A, depois da
M2-10) e para cada tarefa deixar o `ctest` verde: dá para parar em qualquer ponto sem deixar a
engine quebrada. Marque `- [x]` aqui conforme as tarefas terminarem.

---

## 1. Ponto de partida (conferido no código)

- A sala é **cor chapada + guias**: `RoomBackdropRenderer::draw` pinta parede, chão, raios e linhas
  de perspectiva a partir de `Room::wallColor` etc. Não existe imagem de fundo.
- Objeto **sem clip** vira um retângulo colorido com contorno (`EntityRenderer::drawObject`, o
  placeholder que o D10 deixou "até o M2"). Não há como ter uma área clicável invisível.
- Profundidade: `SceneObject::depth()` = `bounds.y + bounds.h` (o pé do retângulo). **Uma só função**
  serve ao desenho (`SceneRenderer`, `std::stable_sort` ascendente, o player entra com `feet.y`) e ao
  picking (`pickInteractiveObject`, `Interaction.cpp:163`, que escolhe na ordem visual inversa).
- Picking só considera objeto **visível e com regra** (`hasRulesForObject`); o ponto aproximado de
  um objeto não sólido é o pé de `bounds` (`interactionApproachPoint`, `Interaction.cpp:183`).
- Câmera: viewport fixa 1280×720 (`World`), sala maior rola; `Camera::interpolatedPosition(alpha)`
  é o deslocamento que todo desenho do mundo subtrai.
- `SpriteLibrary::draw(clip, retângulo destino, tempo)` já desenha PNG inteiro, folha cortada,
  espelhado e com o quadriculado roxo quando falta arte. **Uma camada pode reaproveitar `SpriteClip`
  como descrição da imagem** sem tocar na biblioteca.
- `RoomFiles::load` é uma cadeia de `if` por chave (`RoomFiles.cpp:148-164`); `Room::validationIssue`
  devolve `RoomValidationIssue{subject, index, field}` e o loader só traduz para a linha.
- `ArtCheck::warnings` confere clips de player e de objetos; `DebugOverlay.cpp:86` desenha
  contorno de colisão e ID de cada objeto atrás do editor.
- Estado de partida: `SessionState` guarda visibilidade/colisão/animação **por objeto**. Isso já
  serve a hotspots, desde que **um hotspot seja um objeto** (ver D2).

## 2. Objetivo e fora de escopo

**Objetivo:** o designer pinta uma sala em PNGs (fundo, paralaxe, primeiro plano), coloca áreas
clicáveis invisíveis sobre a arte, e faz o player passar atrás de objetos altos, tudo pelos
arquivos `.room`, com picking e desenho usando a mesma profundidade, avisos de arte ausente e
nenhum desenho de diagnóstico na build de loja.

**Fora de escopo (cada item tem onde entra depois):**

| Fora | Por quê / onde |
| --- | --- |
| Mostrar/esconder camada por script (`show_layer`) | exige campo novo no save (versão 4); ver M2-38 |
| Hotspot poligonal, picking por alfa do PNG | retângulo basta no início; avaliar no M3 (hover) |
| Área caminhável a partir da arte (máscara PNG) | M14 |
| Pré-carga de texturas na troca de sala, fade | M9 |
| Atlas de texturas, tiles gigantes, *culling* avançado | L4 |
| Arrastar camadas/objetos no editor | M16 |

## 3. Decisões de design (D1–D11 aprovadas em 14/11/2026)

| # | Decisão | Motivo |
| --- | --- | --- |
| D1 | **Camada** (`[layer <id>]`) é só visual: imagem, posição, tamanho, `scroll_factor`, `plane = back\|front`. Não entra no picking, na colisão nem na profundidade. | Mantém o contrato "definição não guarda progresso"; interação fica nos objetos. |
| D2 | **Hotspot** (`[hotspot <id>]`) **é um `SceneObject`** com `hotspot = true` (sem clip, não desenha). Divide o espaço de IDs com os objetos. | `show_object`, `set_solid`, `walk_to`, regras, `SessionState` e save já funcionam por `roomId + objectId`; nada de catálogo novo. |
| D3 | Hotspot aceita `type = solid` + `collision` (parede invisível sobre a arte) e `baseline`. Não aceita `color`, `sprite.*`, `animation.*`. | Parede invisível é o uso mais comum depois do clique. |
| D4 | **`baseline`** é relativo ao topo de `bounds` (`depth() = bounds.y + baseline`); ausente = `bounds.h` (comportamento de hoje). Escrito no arquivo só quando definido. | Mover o objeto no editor leva a baseline junto; arquivos antigos salvam idênticos. |
| D5 | `interactionApproachPoint` de objeto não sólido passa a usar `depth()`, ou seja, a baseline. | Picking, desenho e ponto de chegada concordam sobre "onde o objeto pisa". |
| D6 | **Tamanho padrão da camada = tamanho da sala**; `size` e `position` explícitos para o resto. O corte do clip não pode usar `display_width/height/offset_*` (validação recusa). | Independe de abrir o PNG no domínio; um fundo do tamanho da sala não precisa de chave nenhuma. |
| D7 | `scroll_factor = fx, fy`, padrão `1, 1`, intervalo `0 a 4` (maior que 1 = primeiro plano que passa mais rápido). Posição na tela = `position - camera * fator`. | Paralaxe sem estado: só aritmética pura, testável sem janela. |
| D8 | O fundo procedural (parede, chão, guias) **só desenha quando a sala não tem camada `back`**. | Salas antigas ficam idênticas; sala com arte não precisa de chave para desligar o placeholder. |
| D9 | Camadas têm IDs próprios (únicos entre camadas, `validIdentifier`). Erros de camada têm linha (`Subject::layer`). | Mesmo padrão do D12 do roadmap. |
| D10 | Camada **não** muda o save: nenhuma versão nova de `save.slot`. | Só definição; o estado vive nos objetos (D2). |
| D11 | Camada animada e `repeat_x` ficam como **opcionais** (M2-14, M2-15), depois do marco B. | Valem, mas não bloqueiam o resto. |

## 4. Formato alvo (esboço)

```ini
[room]
id = stage
room.width = 2560

# Fundo: sem chaves além do arquivo, cobre a sala inteira e anda com o mundo.
[layer ground]
plane = back
image.file = cenario/stage_ground.png

# Paralaxe: mais estreita que a sala, anda a 20 % da câmera.
# Cobre a tela se size.x >= 1280 + (largura_da_sala - 1280) * fator  ->  1536 <= 1600.
[layer sky]
plane = back
image.file = cenario/stage_sky.png
position = 0, 0
size = 1600, 720
scroll_factor = 0.2, 0

# Primeiro plano: desenha por cima de tudo, inclusive do player.
[layer foliage]
plane = front
image.file = cenario/stage_foliage.png
scroll_factor = 1.3, 1

# Objeto alto: o player passa atrás enquanto os pés estiverem acima de y = 240 + 296.
[object pillar]
bounds = 900,240,96,300
baseline = 296
sprite.file = cenario/stage_pillar.png

# Áreas invisíveis sobre a arte pintada.
[hotspot window]
bounds = 400,60,160,120

[hotspot invisible_wall]
bounds = 1500,300,64,200
type = solid
collision = 0,0,64,200
```

Ordem de desenho final: **fundo** (camadas `back` na ordem do arquivo, ou o placeholder se não há
nenhuma) → **objetos e player** por profundidade → camadas `front` na ordem do arquivo → fala e
menus. Regras de validação e mensagens ficam em `Room::validationIssue`.

## 5. Como trabalhar uma micro-tarefa

1. Abra a linha da tabela de `AGENTS.md` que cobre a mudança e leia "Depois confira".
2. Escreva primeiro o teste que falha (nomes em `TEST_CASE("testNome")`, `REQUIRE`, pasta própria por
   `tests/support/TempDirectory.hpp`); o caso tem de passar sozinho (`--test-case=`).
3. Implemente o mínimo. Respeite as camadas (`pointclick_layer_includes`): `game/` sem SDL de
   desenho, `graphics/` só lê `const Scene&`, diagnóstico só em `debug/`.
4. `cmake --build --preset debug && ctest --preset debug`, depois `--target format-check`.
   Antes de fechar uma fase, rode também `asan` e `store` (ver M2-37).
5. Marque a caixa aqui. Nada de commit automático: o fechamento de cada tarefa é seu para revisar.

**Delegação ao Argon** (ver `CLAUDE.md`): a coluna **Arg.** diz se a tarefa cabe. Só vale quando
toca 3+ arquivos ou é em lote **e** não decide formato nem catálogo. Formato `.room`, validação e
renderer ficam comigo; testes em lote, documentação e arte sintética podem ir.

## 6. Progresso

| Fase | Tarefas | Resultado | Feito |
| --- | --- | --- | --- |
| F0 Preparação | M2-01 a M2-03 | rede de segurança e `ObjectFields` | 1/3 |
| F1 Fundo pintado | M2-04 a M2-10 | **marco A:** um PNG de fundo aparece no lugar do placeholder | 0/7 |
| F2 Paralaxe e primeiro plano | M2-11 a M2-15 | **marco B:** camadas em movimento e à frente do player | 0/5 |
| F3 Profundidade (`baseline`) | M2-16 a M2-19 | player passa atrás de objeto alto | 0/4 |
| F4 Hotspots | M2-20 a M2-25 | áreas invisíveis clicáveis e paredes invisíveis | 0/6 |
| F5 Ferramentas | M2-26 a M2-28 | editor/`--exec`/`--check` enxergam tudo | 0/3 |
| F6 Playground | M2-29 a M2-33 | **marco C:** sala `stage` jogável e testada | 0/5 |
| F7 Documentação e fechamento | M2-34 a M2-38 | docs, fuzz, verificação completa | 0/5 |

---

## 7. Tarefas

### F0 — Preparação

- [x] **M2-01 · S · Arg. não — Rascunho do formato no `GAME_DESIGN.md`.**
  Seção "Camadas, hotspots e profundidade" com o esboço da seção 4 e a lista de chaves, marcada
  "em implementação". **Toca:** `docs/GAME_DESIGN.md`. **Pronto quando:** as decisões D1–D11 estão
  contestadas ou aceitas por você; a seção vira a especificação que as tarefas seguintes cumprem.
- [ ] **M2-02 · M · Arg. sim — Testes de caracterização do que vai mudar.**
  Trava o comportamento de hoje antes de mexer: (a) um frame headless de uma sala sem camadas
  (pixel do centro da parede e do chão, linha de guia); (b) ordem de desenho e de picking de dois
  objetos sobrepostos e do player; (c) `interactionApproachPoint` de objeto não sólido. **Toca:**
  `tests/SpriteTests.cpp`, `tests/GameTests.cpp`. **Pronto quando:** os testes passam **antes** de
  qualquer mudança de `src/` e são o critério de "salas antigas não mudaram" das fases seguintes.
  Conferir antes o que `testGameFrameHasNoDebugDrawing` já cobre, para não duplicar.
- [ ] **M2-03 · M · Arg. não — `ObjectFields`: tabela de campos escalares do objeto.**
  Refatoração que **não muda comportamento**: tira da cadeia de `if` de `RoomFiles::load` e do
  `RoomFiles::save` as chaves `bounds`, `collision`, `color`, `type` e `animation.initial`, em
  uma tabela `{chave, ler(valor, objeto), escrever(out, objeto)}` em `src/content/ObjectFields.*`
  (os campos de sprite continuam em `SpriteFields`). É o que evita que `baseline` e `hotspot`
  virem mais um `if` espalhado (roadmap 2.1, "campo novo de objeto"). **Pronto quando:**
  `RoomFiles::load` + `save` de **cada** sala de `assets/rooms` relê igual (`Room` igual) e o
  arquivo gravado é byte a byte igual ao de antes da refatoração; erros de linha iguais. **Teste:**
  `testObjectFieldsRoundTripAssets` em `tests/EditorTests.cpp`. **Corte de segurança:** se passar
  de um dia, abandone a tabela e adicione `baseline` como um `if` (M2-17); anote aqui.

### F1 — Fundo pintado (marco A)

- [ ] **M2-04 · M · Arg. não — Modelo `RoomLayer` e a aritmética pura.**
  `src/game/RoomLayer.hpp/.cpp`: `enum class LayerPlane { back, front }`, `struct RoomLayer { id,
  plane, SpriteClip image, SDL_FPoint position, SDL_FPoint size, SDL_FPoint scrollFactor }`;
  `Room::layers` e `Room::findLayer(id)`; função pura `layerRect(layer, room, cameraOffset)`
  (tamanho zero = tamanho da sala, D6; `position - camera * fator`, D7) e
  `layerCoversViewport(layer, room)`. Acrescentar `RoomLayer` à lista de definições em
  `cmake/CheckLayerIncludes.cmake`. **Pronto quando:** compila, sem SDL de desenho em `game/`.
  **Teste:** `testLayerRect` (fator 0, 0,5 e 1; câmera em 0 e no máximo; tamanho padrão),
  `testLayerCoversViewport` (a conta do exemplo: 1600 cobre, 1500 não) em `tests/GameTests.cpp`.
- [ ] **M2-05 · M · Arg. não — Validação das camadas.**
  `Room::validationIssue`: ID válido e único entre camadas; `size` e `scroll_factor` finitos e
  positivos / em `0..4`; `position` finita; clip passa em `SpriteClip::validationError`; clip sem
  `display_width/height/offset_*` (D6); camada sem `image.file` é aviso de arte, não erro (igual
  clip sem arquivo hoje). Novo `RoomValidationIssue::Subject::layer`. **Pronto quando:** cada regra
  tem mensagem própria e `field` ("image.file", "size", "scroll_factor"...). **Teste:**
  `testLayerValidation` em `tests/EditorTests.cpp`, um caso por regra.
- [ ] **M2-06 · M · Arg. não — Leitura e gravação de `[layer]` no `.room`.**
  `RoomFiles::load`: cabeçalho `[layer <id>]`, chaves `plane`, `position`, `size`,
  `scroll_factor` e `image.*` (reaproveita `SpriteFields::parse` com o prefixo); `layerLines`
  para o número de linha (`Linha N:`), ID repetido recusado no cabeçalho, chave repetida e chave
  desconhecida como nos objetos. `RoomFiles::save` grava as camadas **depois dos spawns e antes
  dos objetos**, só com as chaves fora do padrão. **Pronto quando:** erros têm linha e o `save`
  relê igual. **Teste:** `testRoomLayerFiles` em `tests/EditorTests.cpp` (round-trip, linha de cada
  erro, `scroll_factor` com vírgula, `plane` inválido).
- [ ] **M2-07 · S · Arg. sim — Salas antigas continuam idênticas.**
  Prova do D10/D8 no formato: `load` + `save` de todas as salas de `assets/rooms` não acrescenta
  nenhuma chave de camada; o `.room` regravado é igual ao que a M2-03 gravava. **Teste:**
  estender `testObjectFieldsRoundTripAssets` (ou `testRoomLayerFiles`) com "nenhum `[layer`" no
  texto. **Pronto quando:** passa também sem a M2-03 (compara com o texto original).
- [ ] **M2-08 · M · Arg. não — `LayerRenderer` e o fundo condicional.**
  `src/graphics/LayerRenderer.hpp/.cpp`: `draw(renderer, sprites, room, plane, cameraOffset)` com
  `layerRect` + `SpriteLibrary::draw`. `SceneRenderer::draw`: se `room.layers` tem alguma `back`,
  desenha só as camadas; senão chama `RoomBackdropRenderer::draw` (D8). Objetos e player depois,
  sem mudança. **Pronto quando:** uma sala de teste com uma camada `back` mostra o PNG. **Teste:**
  M2-09.
- [ ] **M2-09 · M · Arg. sim — Testes de frame do fundo.**
  Frame headless (renderer de software, como `testGameFrameHasNoDebugDrawing`): (a) sala sem
  camadas = pixels da M2-02; (b) com camada `back` o pixel onde o placeholder tinha parede/chão/
  guia é a cor do PNG; (c) PNG ausente = quadriculado roxo, a sala carrega e roda (`testRoomWithMissingArtStillRuns`
  ganha o caso); (d) uma camada que cobre só metade deixa o resto do fundo limpo (cor de limpeza do
  `Renderer`, não o placeholder). PNGs gerados em `TempDirectory` por `writeSheetPng`. **Toca:**
  `tests/SpriteTests.cpp`.
- [ ] **M2-10 · M · Arg. não — `ArtCheck` e `--check` para camadas.**
  `ArtCheck::warnings` confere o clip de cada camada (PNG ausente, ilegível, corte que não cabe,
  clip sem arquivo; dono = ID da camada) e **avisa quando a camada não cobre a tela em todas as
  posições da câmera** (`layerCoversViewport`, mensagem com o tamanho mínimo). Continua sendo
  `aviso:` e sai 0. **Toca:** `src/content/ArtCheck.cpp`, `tests/fixtures/missing_art`.
  **Teste:** `testArtWarnings` (camadas) em `tests/SpriteTests.cpp`, `testCheckWarnsAboutMissingArt`
  em `tests/CommandLineTests.cpp`. **Marco A:** abrir o jogo com uma sala de teste e ver o fundo.

### F2 — Paralaxe e primeiro plano (marco B)

- [ ] **M2-11 · S · Arg. sim — Teste de paralaxe em frame.**
  Câmera deslocada 400 px: camada fator 1 anda 400, fator 0,5 anda 200, fator 0 fica parada;
  pixel de uma marca no PNG confere a posição. Interpolação (`alpha` 0 e 1) usa o mesmo
  `interpolatedPosition` do mundo. **Toca:** `tests/SpriteTests.cpp`.
- [ ] **M2-12 · M · Arg. não — Plano `front` por cima do player.**
  `SceneRenderer`: camadas `front` depois do laço de objetos e player. **Teste:** pixel sobre o
  player é o da camada `front`; sob a camada `back` o player aparece. **Pronto quando:** a ordem
  fundo → objetos/player → front vale com o player andando.
- [ ] **M2-13 · S · Arg. não — *Culling* simples de camada.**
  `LayerRenderer` pula camada cujo retângulo na tela não cruza a viewport (`SDL_HasRectIntersectionFloat`
  ou conta própria). **Teste:** `layerRect` fora da tela não chama o desenho (contador via sala com
  PNG ausente: nenhum quadriculado quando fora). **Pronto quando:** nenhum desvio visual.
- [ ] **M2-14 · M · Arg. não — (opcional) Camada animada.**
  Relógio de sala determinístico em `Scene` (`roomTime`, zera em `apply` com `resetPlayer`, soma
  em `tick`); `LayerRenderer` passa o tempo ao `SpriteLibrary::draw`; clip com `frames`/`fps`.
  **Teste:** dois instantes diferentes dão quadros diferentes; o relógio não entra no save.
  Decidir **depois do marco B** se vale agora.
- [ ] **M2-15 · M · Arg. não — (opcional) `repeat_x` para céus e chãos infinitos.**
  Chave `repeat_x = 0|1`: desenha a imagem lado a lado cobrindo a viewport (módulo da largura).
  Validação: só com `size.x > 0`. **Teste:** pixel em duas posições separadas por `size.x` é igual.
  Só faz sentido com paralaxe; pode ficar para depois do M3.

### F3 — Profundidade (`baseline`)

- [ ] **M2-16 · S · Arg. não — Campo e validação.**
  `SceneObject::baseline` (`std::optional<float>`); `depth()` = `bounds.y + baseline.value_or(bounds.h)`.
  `Room::validationIssue`: `0 <= baseline <= bounds.h`, campo "baseline". **Teste:**
  `testObjectBaselineValidation` em `tests/EditorTests.cpp`.
- [ ] **M2-17 · S · Arg. não — Chave `baseline` no `.room`.**
  Leitura, gravação só quando definida (arquivos antigos salvam iguais), erro com linha. Se a
  M2-03 foi feita, é uma entrada na tabela do `ObjectFields`; senão, um `if`. **Teste:**
  `testObjectBaselineFiles` (round-trip, valor fora da faixa com `Linha N:`).
- [ ] **M2-18 · M · Arg. não — Picking e ponto de chegada concordam com a baseline.**
  `pickInteractiveObject` já usa `depth()`; conferir e provar. `interactionApproachPoint` de objeto
  não sólido passa a usar `depth()` (D5). **Teste (`tests/GameTests.cpp`):** dois objetos
  sobrepostos com baselines invertidas escolhem o da frente; objeto com baseline curta tem ponto de
  chegada acima do pé de `bounds`; sala sem baseline com os mesmos números da M2-02.
- [ ] **M2-19 · M · Arg. sim — Frame: player atrás e na frente.**
  Player com `feet.y` acima da baseline do objeto aparece **atrás** (pixel do objeto sobre o
  player); abaixo, **na frente**. Empate mantém a regra do `stable_sort`. **Toca:**
  `tests/SpriteTests.cpp`.

### F4 — Hotspots

- [ ] **M2-20 · M · Arg. não — Modelo, validação e `[hotspot]` no `.room`.**
  `SceneObject::hotspot`; `RoomFiles::load` aceita `[hotspot <id>]` (mesmo espaço de IDs dos
  objetos, `objectLines` compartilhado, chaves `bounds`, `type`, `collision`, `baseline`); chave de
  arte (`color`, `sprite.*`, `animation.*`) é erro com linha; `Room::validationIssue` recusa
  hotspot com animações. `save` escreve `[hotspot id]` quando `hotspot` e não tem clips.
  **Teste:** `testHotspotFiles` em `tests/EditorTests.cpp` (round-trip, ID repetido entre
  `[object]` e `[hotspot]`, chave de arte recusada com a linha).
- [ ] **M2-21 · S · Arg. não — Render ignora hotspot.**
  `SceneRenderer`/`EntityRenderer::drawObject`: hotspot não entra na lista de desenho. **Teste:**
  `testHotspotDrawsNothing` (frame: nenhum pixel do placeholder, nem contorno, em cima do hotspot).
  **Pronto quando:** `testGameFrameHasNoDebugDrawing` continua passando.
- [ ] **M2-22 · M · Arg. sim — Comportamento de jogo do hotspot.**
  `Scene::pick`, `walk_to`, `show_object`/`hide_object`, `set_solid`, regras e `[use_item item
  hotspot]` funcionam; hotspot `solid` bloqueia a rota; oculto não é escolhido. **Teste
  (`tests/GameTests.cpp`):** clicar no hotspot dispara a regra; `hide_object` o remove do picking;
  parede invisível desvia o caminho; `set_animation` num hotspot recusa com mensagem clara.
- [ ] **M2-23 · M · Arg. não — `show hotspots` no editor.**
  `DebugViews` ganha `hotspots`; uma entrada em `kViewSwitches` (`show hotspots 0|1`) e o desenho
  (contorno + ID, e a cor diferente para oculto) em `src/debug/DebugOverlay.cpp`, **nunca** em
  `graphics/`. Sem o editor ativo e na build de loja, nada é desenhado. **Teste:**
  `testHotspotView` em `tests/EditorTests.cpp` (`help`, valores, quadro com e sem a visão) e
  `testGameFrameHasNoDebugDrawing` com um hotspot na sala.
- [ ] **M2-24 · S · Arg. sim — `--check` sem regra órfã.**
  Aviso `aviso:` quando um hotspot não tem **nenhuma** regra na sala (pronto para o designer
  esquecer o `.rules`); o objeto comum sem regra continua sem aviso. **Toca:**
  `src/content/ContentCheck.cpp`. **Teste:** `tests/CommandLineTests.cpp`, fixture nova.
- [ ] **M2-25 · S · Arg. não — Mensagens de erro de hotspot no `--check`.**
  Percorrer `--check` com salas quebradas de propósito (hotspot sem `bounds`, `solid` sem
  `collision`, `baseline` negativa) e ajustar os textos. **Teste:** `tests/fixtures/broken_rooms`
  ganha os casos; `pointclick_check_broken_rooms` continua falhando de propósito.

### F5 — Ferramentas

- [ ] **M2-26 · M · Arg. sim — `save`/`reload`/`undo` do editor com camadas e hotspots.**
  `save` do editor grava `[layer]`, `[hotspot]` e `baseline`; `reload` recarrega o PNG da camada
  (o recarregador de arte limpa o `SpriteLibrary`); `undo`/`reset` restauram a sala inteira.
  **Teste:** `tests/EditorTests.cpp`, um caso por comando.
- [ ] **M2-27 · M · Arg. não — (opcional) `show layers` e comando `layers`.**
  Visão que contorna cada camada com ID e fator na posição em que ela está sendo desenhada;
  comando `layers` lista id, plano, fator e tamanho. Um catálogo, duas entradas em `kCommands`/
  `kViewSwitches`. **Teste:** `testCommandCatalogHelp` cobre `help`; um caso por comando.
- [ ] **M2-28 · S · Arg. sim — `--exec` com camadas.**
  `--exec "layers" --exec state` roda num jogo novo e imprime a lista; `ctest`
  `pointclick_exec_*` ganha um caso com a sala `stage` (depois da M2-30). **Toca:**
  `CMakeLists.txt`, `tests/CommandLineTests.cpp`.

### F6 — Playground e percurso (marco C)

- [ ] **M2-29 · M · Arg. sim — Arte sintética do `stage`.**
  `scripts/make_playground_art.py` gera `assets/sprites/cenario/stage_*.png` (céu em degradê, colinas,
  chão com marcas a cada 128 px para ver o deslocamento, folhagem, pilar): Python puro, os mesmos
  bytes em qualquer máquina; `--check` compara com o disco (o `ctest` `pointclick_playground_art`
  já roda isso). Atualizar `assets/sprites/README.md`. **Pronto quando:** `--check` passa e as
  imagens têm tamanhos coerentes com a sala (M2-04: 2560×720, céu 1600×720...).
  **Já existe** (14/11/2026) um conjunto de placeholders feito à mão em `assets/sprites/cenario/test_<LxA>_<camada>.png`
  (1280×720, 2560×720, 2560×1080 e 4673×1620, mais `test_96x300_pillar.png`): servem aos testes e
  à sala `stage` até esta tarefa; não são gerados pelo script, então o `--check` do script não os cobre.
- [ ] **M2-30 · M · Arg. não — Sala `stage` e suas regras.**
  `assets/rooms/stage.room` (2560×720, `[spawn west]`/`[spawn east]`, camadas `sky`, `hills`,
  `ground`, `foliage`; objeto `pillar` com `baseline`; hotspots `window` e `invisible_wall` sólido)
  e `stage.rules` (`window` → `say`, `pillar` → `say`, `[use_item * invisible_wall]` curinga se
  couber). **Pronto quando:** `pointclick --rooms assets/rooms --room stage west` abre e a câmera
  rola; `--check` sem `aviso:`.
- [ ] **M2-31 · M · Arg. sim — Percurso jogado da `stage`.**
  `testWalkthroughStage` em `tests/WalkthroughTests.cpp` com o `WalkthroughDriver`: clicar no
  hotspot (`clickObject`), conferir o `say`; andar de um lado ao outro com a parede invisível no
  caminho (a rota desvia, `isObjectSolid`); ficar acima e abaixo da baseline do pilar e conferir
  `depth()` contra `player().feet().y`; `saveAndResume` mantém tudo. **Pronto quando:** passa sozinho
  (`--test-case=`) e em ordem aleatória.
- [ ] **M2-32 · M · Arg. sim — Pixels da `stage` em frame headless.**
  Em `tests/SpriteTests.cpp`: frame da sala real com a câmera no início e no fim da sala; o céu
  (fator 0,2) anda menos que o chão (fator 1); a folhagem (fator 1,3) cobre o player na posição
  marcada; o pilar cobre o player só acima da baseline. Mutação conferida: trocar `plane` ou
  `scroll_factor` de uma camada falha o teste.
- [ ] **M2-33 · S · Arg. sim — Janela real (manual).**
  Abrir `pointclick --rooms assets/rooms --room stage west` numa janela Metal e conferir: nenhuma
  costura nas bordas das camadas ao rolar, paralaxe suave com o VSync, `show hotspots 1` mostra os
  contornos, o quadriculado roxo aparece ao renomear um PNG. Registrar em `docs/ROADMAP.md`, seção
  3.3 (execução em janela real, como as verificações do F2).

### F7 — Documentação e fechamento

- [ ] **M2-34 · M · Arg. sim — `GAME_DESIGN.md` definitivo.**
  A seção da M2-01 vira o guia final: chaves, validações, receita "pintar uma sala em três
  passos", dicas (tamanho mínimo da camada, hotspot sobre paralaxe só alinha com o fator 1), linha do
  `stage` na tabela de playgrounds, comandos novos na tabela do editor.
- [ ] **M2-35 · S · Arg. sim — `ARCHITECTURE.md`, `AGENTS.md`, `README.md`.**
  Módulos novos (`game/RoomLayer`, `graphics/LayerRenderer`, `content/ObjectFields` se feito),
  ordem de desenho, e as linhas do mapa do `AGENTS.md`: "Camadas e composição", "Aparência da
  sala", um item novo "Camada, hotspot ou `baseline`" apontando para `Room.hpp`, `RoomFiles.cpp`,
  `LayerRenderer.cpp`, `tests/EditorTests.cpp`. Checar que o contrato "diagnóstico só em `debug/`"
  segue valendo.
- [ ] **M2-36 · S · Arg. sim — Sementes do fuzz.**
  `tests/fuzz/ParserFuzz.cpp` ganha sementes com `[layer]`, `[hotspot]`, `baseline`,
  `scroll_factor` com vírgula e valores absurdos. Rodar
  `./build/asan/pointclick_parser_fuzz 20000` sem crash nem relatório.
- [ ] **M2-37 · M · Arg. não — Verificação completa.**
  `debug`, `asan` e `store` verdes (a loja não deve ter nenhum símbolo de `show hotspots`:
  conferir com `nm`), `format-check`, `pointclick --rooms assets/rooms --check` sem `aviso:`, fuzz,
  sem avisos do compilador. Anotar contagem de testes e data no `docs/ROADMAP.md`.
- [ ] **M2-38 · S · Arg. não — Fechar o M2 no `ROADMAP.md`.**
  Mover o M2 para a seção 3.1 (com a tabela de "como ficou" e os testes), atualizar o grafo da
  seção 7, o passo 4 da 4.4 e as dependências do M3/M4. Abrir, se valer, um item novo para o que
  ficou de fora (`show_layer` com save v4, hotspot poligonal) e para as opcionais não feitas
  (M2-14, M2-15, M2-27).

---

## 8. Riscos e armadilhas

| Risco | O que fazer |
| --- | --- |
| **Textura grande demais**: uma sala de 8192 px de largura pode estourar o limite de textura da GPU em celular (muitos aceitam 4096). | Aviso no `ArtCheck` para PNG de camada acima de 4096 px de lado (junto da M2-10); tiles ficam para o L4. Os playgrounds ficam abaixo disso. |
| **Engasgo no primeiro desenho** de um PNG grande (a carga é síncrona em `SpriteLibrary::imageFor`). | Já é o M9; não mexer aqui. Anotar o tempo medido na M2-33. |
| **Costura entre camadas** por coordenada fracionária com a câmera interpolada. | `SDL_RenderTexture` com `float`; conferir na janela real (M2-33). Se aparecer, arredondar o deslocamento da camada ao pixel. |
| **Salas antigas mudando em silêncio** (placeholder, profundidade, ponto de chegada). | M2-02 trava o comportamento antes; M2-07 prova que o arquivo regravado é igual. |
| **Hotspot sobre camada com paralaxe** não acompanha a imagem (picking é em coordenadas do mundo, fator 1). | Documentar na M2-34; regra de ouro: hotspot casa com a camada de fator 1. |
| **Aggregate init** de `SceneObject` em testes e no loader (`{{}, cor, tipo, {}, id, {}, "idle"}`). | Campos novos entram **no fim** da struct com valor padrão; compilar todos os alvos depois de cada mudança de struct. |
| **Editor descarta comentários do `.room`** ao salvar (M16). | Sem tratar aqui; avisar no `GAME_DESIGN.md` que o `save` do editor reescreve o arquivo. |
| **Ordem de camadas no arquivo** vira significado (quem vem depois desenha por cima). | Documentar; `save` preserva a ordem do arquivo. |

## 9. Definição de pronto do M2

- Uma sala só com PNGs (fundo, paralaxe, primeiro plano, objetos com `baseline`, hotspots) joga de
  ponta a ponta no `stage`, com percurso e frame testados.
- `debug` 100 %, `asan` sem relatório, `store` sem símbolo de diagnóstico, `format-check` limpo,
  `--check` sem `aviso:`, fuzz sem crash.
- Nenhuma sala antiga mudou de arquivo regravado nem de pixel (M2-02/M2-07).
- Arte ausente continua sendo aviso e quadriculado roxo, nunca erro nem parada.
- `docs/ROADMAP.md` registra o que foi feito e o que ficou de fora; este arquivo fica como histórico
  com todas as caixas marcadas.
