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
| `game/Game` | Coordena entrada de gameplay, salas, navegação e jogador |
| `game/Player` | Posição atual/anterior e execução dos pontos da rota |
| `game/Navigation` | Grade, obstáculos, A* e simplificação segura das rotas |
| `debug/DebugEditor` | Reconhece DEBUG, processa comandos e gerencia edição/undo |
| `graphics/RoomBackdropRenderer` | Parede, chão e linhas de perspectiva |
| `graphics/EntityRenderer` | Desenho do player e dos objetos |
| `graphics/SceneRenderer` | Ordenação por profundidade e composição da cena |
| `graphics/SpriteLibrary` | Carregamento de PNG, cache de texturas, corte e imagem quadriculada de substituição |
| `graphics/DiagnosticsOverlay` | Textos técnicos de posição e viewport |
| `graphics/DebugOverlay` | Interface visual do editor |
| `graphics/Renderer` | Ciclo de desenho e apresentação lógica do SDL |
| `core/Application` | Janela, eventos SDL e atualização em passo fixo |

O executável e os testes compartilham `pointclick_core`. As rotinas de desenho
ficam em `pointclick_graphics`. Uma correção no jogo chega aos dois consumidores.
As dependências seguem `core` → `debug`/`content`/`graphics` → `game`.
`graphics` e `content` leem tipos de `game`; o domínio do jogo não carrega
arquivos de arte nem chama funções de desenho.

## Fluxos principais

Clique → conversão janela/mundo → `Game::onClick` → navegação → rota do `Player`.
A cada tick, o jogo calcula a velocidade na profundidade atual; o player avança
sem ultrapassar o próximo ponto. O player escolhe a animação pela direção do
movimento e reinicia seus quadros ao mudar de direção. O render usa posições
interpoladas.

Texto → `DebugEditor::onTextInput` → ativação por DEBUG. Enquanto ativo, o editor
consome texto e teclas de edição. As alterações passam por `Game::applyRoom`;
ele valida os dados e prepara uma nova navegação antes de substituir o estado.
Uma edição rejeitada não altera sala, posição ou rota. Uma edição aceita
interrompe a rota, preserva os pés quando possível e procura chão livre quando
necessário. Uma sala inteiramente bloqueada é rejeitada.

`save` grava primeiro um arquivo temporário e depois o substitui no destino.
`load` passa pela mesma validação usada pelas edições. A aplicação carrega
`first.room` da pasta indicada por `--rooms` na inicialização.

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

O canvas lógico é compartilhado por todas as salas; o SDL aplica letterbox e
converte os cliques. A navegação usa uma grade retangular com limite superior
de chão. Formatos de chão com polígonos, múltiplos andares e obstáculos móveis
exigirão ampliar esse modelo. O formato `.room` não executa scripts.

O player é um ponto nos pés para a navegação, com margens configuráveis em volta
dos obstáculos. A largura visual varia com a perspectiva; ajuste as margens
quando mudar essa largura. Não há colisão física por pixel de sprite.

## Verificação

```sh
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
```

Os testes cobrem movimento, perspectiva, desvio, quinas, configurações inválidas,
ativação e fechamento do editor, edição em tempo real, persistência, outra sala
e conversão de coordenadas ao redimensionar. Os testes de sprites cobrem cortes,
animação, seleção de direção, persistência e imagem de substituição. Os testes gráficos
usam o driver virtual e o renderizador de software do SDL. As verificações também
ficam ativas em Release.
