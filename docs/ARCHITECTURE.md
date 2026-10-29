# Organização e extensão

A configuração da sala é um conjunto de dados. O estado do jogo, a entrada,
a renderização e as ferramentas de edição têm responsabilidades separadas.

| Local | Responsabilidade |
| --- | --- |
| `assets/rooms/*.room` | Salas editáveis por game design |
| `assets/sprites/*.png` | Imagens estáticas e folhas de animação |
| `game/RoomParameters` | Catálogo de parâmetros: nome, descrição, intervalo, passo e acesso ao campo |
| `content/RoomFiles` | Leitura e gravação das salas; erros com número de linha |
| `content/SpriteFields` | Leitura e gravação dos campos de um clip, independente da sala |
| `content/RoomPresets` | Sprites padrão e exemplo embutido usado por testes e por quem instancia `Game` diretamente |
| `game/Room` | Dados da sala, validação, escala, velocidade e limites |
| `game/SceneObject` | Identidade, desenho, tipo e base local de colisão |
| `game/SpriteClip` | Definição de arquivo, corte, tempo e âncora visual de um sprite |
| `game/Animation` | Clips nomeados e reprodução por entidade |
| `game/ObjectAnimations` | Estado de reprodução dos objetos, por ID, preservado nas edições |
| `game/Camera` | Câmera 2D suave com campo de visão fixo (`1280×720`), clamp nas bordas da sala e interpolação |
| `game/Game` | Coordena entrada de gameplay, salas, navegação, câmera e jogador |
| `game/Player` | Posição atual/anterior e execução dos pontos da rota |
| `game/Navigation` | Grade dinâmica por sala, obstáculos, A* e simplificação segura das rotas |
| `debug/DebugEditor` | Reconhece DEBUG, processa comandos e gerencia edição/undo |
| `graphics/RoomBackdropRenderer` | Parede, chão e linhas de perspectiva deslocados pela câmera |
| `graphics/EntityRenderer` | Desenho do player e dos objetos deslocados pela câmera |
| `graphics/SceneRenderer` | Ordenação por profundidade e composição da cena no espaço da câmera |
| `graphics/SpriteLibrary` | Carregamento de PNG, cache de texturas, corte e imagem quadriculada de substituição |
| `graphics/DiagnosticsOverlay` | Textos técnicos de posição, sala, câmera e viewport fixa |
| `graphics/DebugOverlay` | Guias de objetos no mundo e painel visual fixo do editor |
| `graphics/Renderer` | Ciclo de desenho, apresentação lógica `1280×720` com letterbox e conversão janela/viewport/mundo |
| `core/Application` | Janela, eventos SDL e atualização em passo fixo |

O executável e os testes compartilham `pointclick_core`. As rotinas de desenho
ficam em `pointclick_graphics`. Uma correção no jogo chega aos dois consumidores.
As dependências seguem `core` → `debug`/`content`/`graphics` → `game`.
`graphics` e `content` leem tipos de `game`; o domínio do jogo não carrega
arquivos de arte nem chama funções de desenho.

## Fluxos principais

Clique → `Renderer::windowToViewport` (teste de letterbox e `DebugOverlay::panel` em `0..1280 × 0..720`) → `Camera::viewportToWorld` → `Game::onClick` → navegação → rota do `Player`.
A cada tick, o jogo calcula a velocidade na profundidade atual; o player avança
sem ultrapassar o próximo ponto e a `Camera` atualiza sua posição suavizada dentro
dos limites `[0, room.width - 1280] × [0, room.height - 720]` (permanecendo fixa em
`(0, 0)` nas salas `1280×720`). Ao carregar/resetar uma sala ou teleportar o player,
a câmera faz *snap* imediato ao alvo; em edições ao vivo do editor, preserva a
continuidade respeitando os novos limites. O render usa posições interpoladas tanto
para o `Player` quanto para a `Camera`.

Texto → `DebugEditor::onTextInput` → ativação por DEBUG. Enquanto ativo, o editor
consome texto e teclas de edição. As alterações passam por `Game::applyRoom`;
ele valida os dados e prepara uma nova navegação antes de substituir o estado.
Uma edição rejeitada não altera sala, posição ou rota. Uma edição aceita
interrompe a rota, preserva os pés quando possível e procura chão livre quando
necessário. Uma sala inteiramente bloqueada é rejeitada.

`save` grava primeiro um arquivo temporário e depois o substitui no destino.
`load` passa pela mesma validação usada pelas edições. A aplicação carrega
`first.room` da pasta indicada por `--rooms` na inicialização.

A leitura usa `SDL_IOFromFile` e `SDL_LoadFile_IO` do SDL3 3.4.16, com caminhos
UTF-8 e o tamanho explícito dos dados. O stream permanece aberto para conferir
erros de leitura antes do parser; stream e buffer são liberados por RAII.
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

## Adicionar uma mecânica

Use o ID de `SceneObject` para identificar o alvo. Por exemplo, uma porta pode
ser `library_door`; ela mantém a identidade mesmo se o designer mudar a posição.

Coloque a decisão de gameplay em `Game` ou em um componente de domínio chamado
por ele. Um sistema de interação pode receber objeto, ação e estado do jogo,
validar condições e devolver um resultado. Inventário, diálogo e quests devem
ter estados próprios; não devem ser armazenados no renderizador ou no editor.

Para transições de sala, leia a definição com `RoomFiles::load`, confira o erro
e aplique com `Game::applyRoom(room, true, error)`. Para ajustes que preservem a
posição atual, use `false`. Isso reutiliza as verificações e a reconstrução da
navegação. O editor é um consumidor dessa API, não um caminho exclusivo de mudança.

Para adicionar arte, configure um `SpriteClip` no conjunto de animações da sala
ou do objeto. A definição guarda o caminho relativo e o corte da imagem, sem carregar texturas. O
`SpriteLibrary` carrega PNGs sob `assets/sprites`, mantém as texturas em cache e
desenha o quadriculado de substituição quando o PNG ou corte falha. Ao aplicar
uma sala, o renderizador limpa o cache para permitir recarregar arquivos. A
colisão usa `bounds` e a base, independentemente dos pixels do sprite.

## Evoluir animações, interações e diálogos

`SpriteClip` é somente a **definição** de uma animação: arquivo, corte, FPS e
tamanho visual. `AnimationSet` reúne clips por nome. `AnimationPlayback` guarda
o nome ativo e o tempo de uma instância. O `Player` usa esse estado para a
locomoção, e `Game` mantém um estado separado por ID de objeto animado. Uma
regra pode chamar `Game::playObjectAnimation` para selecionar ou reiniciar um
clip; o renderizador só consulta o estado. O tempo é preservado quando o editor
reaplica uma sala sem mudar o objeto ou remover o clip ativo. Carregar uma sala
com reposicionamento reinicia as animações dos objetos.

Os cinco nomes de locomoção do jogador (`idle`, `left`, `right`, `up`, `down`)
são escolhidos em `Player.cpp`. O arquivo `.room` aceita clips adicionais com
`player.NOME.*`, e objetos aceitam `animation.NOME.*`. Para uma ação do jogador,
uma futura regra precisará decidir quando a animação toma prioridade da
locomoção e quando devolve o controle. Para ações de objetos, use o ID do
objeto e `Game::playObjectAnimation`; a conclusão de um clip sem repetição pode
ser consultada com `AnimationPlayback::finished`. Não use um novo
valor de `SceneObjectType` para cada animação: esse enum expressa apenas se a
base do objeto bloqueia o caminho.

Uma interação pode começar com um teste de alvo antes da rota em
`Game::onClick`: identificar o objeto pelo ID, encontrar uma posição alcançável
perto dele e caminhar. Ao chegar, uma regra de interação decide o resultado e
pode iniciar animação, diálogo, mudança de estado ou transição de sala. Assim,
cliques no chão continuam sendo pedidos de movimento; cliques em objetos podem
produzir ações sem colocá-las dentro de `SceneRenderer` ou `DebugEditor`.

Para diálogos, mantenha texto, escolhas, condições e progresso em um sistema de
domínio próprio. A interface de diálogo apenas apresenta esse estado e envia a
escolha do jogador. `Application` deverá encaminhar entrada conforme o modo
ativo (jogo, diálogo ou editor); no momento, texto fora do DEBUG só alimenta o
texto flutuante de demonstração. Uma transição de sala deve ser uma operação de
`Game` que aplique a definição da próxima sala e a posição de entrada. Efeitos
visuais de transição podem usar um estado temporizado separado no renderizador,
sem alterar colisões ou conteúdo da sala.

`Game::applyRoom` é usado hoje pelo editor e reinicia a rota e o `Player` ao
aplicar uma edição. Antes de adicionar progresso persistente, separe o estado
de sessão (inventário, diálogos concluídos, estado dos objetos) da definição
`Room`; editar parâmetros da sala não deve apagar esse progresso. O `undo` do
editor continua sendo uma ferramenta para definição de sala, não para desfazer
ações de gameplay.

## Escopo atual

A viewport lógica da câmera é estritamente fixa em `1280 × 720` (`World::width × World::height`)
com `SDL_LOGICAL_PRESENTATION_LETTERBOX`: monitores 2K, 4K, Ultrawide, celulares e
consoles exibem exatamente o mesmo enquadramento do mundo, sem revelar áreas extras.
Cada sala define suas próprias dimensões (`room.width` e `room.height`, mínimo
`1280 × 720`); salas maiores rolam com `Camera` 2D suave e redimensionam a grade
retangular de `Navigation`. Formatos de chão com polígonos, múltiplos andares e
obstáculos móveis exigirão ampliar esse modelo. O formato `.room` não executa scripts.

O player é um ponto nos pés para a navegação, com margens configuráveis em volta
dos obstáculos. A largura visual varia com a perspectiva; ajuste as margens
quando mudar essa largura. Não há colisão física por pixel de sprite.

## Verificação

```sh
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

Os testes cobrem movimento, perspectiva, desvio, quinas, salas panorâmicas,
câmera fixa e suave com clamp nas bordas, configurações inválidas, ativação e
fechamento do editor, edição em tempo real, persistência, outra sala e
independência de resolução/campo de visão (720p, 1080p, 2K, 4K e Ultrawide).
Os testes de sprites cobrem cortes, animação, seleção de direção, persistência e
imagem de substituição. Os testes gráficos usam o driver virtual e o renderizador
de software do SDL. As verificações também ficam ativas em Release.
