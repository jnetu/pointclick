# Organização e extensão

A configuração da sala é um conjunto de dados. O estado do jogo, a entrada,
a renderização e as ferramentas de edição têm responsabilidades separadas.

| Local | Responsabilidade |
| --- | --- |
| `assets/rooms/*.room` | Salas editáveis por game design |
| `assets/sprites/*.png` | Imagens estáticas e folhas de animação |
| `content/RoomParameters` | Catálogo de parâmetros: nome, descrição, intervalo, passo e acesso ao campo |
| `content/RoomFiles` | Leitura e gravação das salas; erros com número de linha |
| `content/RoomPresets` | Exemplo embutido usado por testes e por quem instancia `Game` diretamente |
| `game/Room` | Dados da sala, validação, escala, velocidade e limites |
| `game/SceneObject` | Identidade, desenho, tipo e base local de colisão |
| `game/SpriteClip` | Definição de arquivo, corte, tempo e âncora visual de um sprite |
| `game/Game` | Coordena entrada de gameplay, salas, navegação e jogador |
| `game/Player` | Posição atual/anterior e execução dos pontos da rota |
| `game/Navigation` | Grade, obstáculos, A* e simplificação segura das rotas |
| `debug/DebugEditor` | Reconhece DEBUG, processa comandos e gerencia edição/undo |
| `graphics/SceneRenderer` | Cenário e ordenação por profundidade |
| `graphics/SpriteLibrary` | Carregamento de PNG, cache de texturas, corte e imagem quadriculada de substituição |
| `graphics/DiagnosticsOverlay` | Textos técnicos de posição e viewport |
| `graphics/DebugOverlay` | Interface visual do editor |
| `graphics/Renderer` | Ciclo de desenho e apresentação lógica do SDL |
| `core/Application` | Janela, eventos SDL e atualização em passo fixo |

O executável e os testes compartilham `pointclick_core`. As rotinas de desenho
ficam em `pointclick_graphics`. Uma correção no jogo chega aos dois consumidores.

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
2. Registre nome, descrição, intervalo e passo em `RoomParameters.cpp`.
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

Para adicionar arte, configure `SpriteClip` na sala ou no objeto. A definição
guarda o caminho relativo e o corte da imagem, sem carregar texturas. O
`SpriteLibrary` carrega PNGs sob `assets/sprites`, mantém as texturas em cache e
desenha o quadriculado de substituição quando o PNG ou corte falha. Ao aplicar
uma sala, o renderizador limpa o cache para permitir recarregar arquivos. A
colisão usa `bounds` e a base, independentemente dos pixels do sprite.

## Evoluir animações, interações e diálogos

`SpriteClip` é somente a **definição** de uma animação: arquivo, corte, FPS e
tamanho visual. O tempo e a animação ativa pertencem ao estado da entidade. O
`Player` já mantém a direção e o tempo da animação de locomoção; a seleção do
clip correspondente fica em `PlayerSprites::forPose`. Os objetos com sprite
usam hoje `Game::sceneTime()` para animações ambientais contínuas. Eles ainda
não possuem estado individual de animação.

Quando houver animações de ação, como abrir uma porta ou pegar um item, crie um
conjunto de clips nomeados por entidade e um estado de reprodução por instância
do objeto (clip ativo, tempo e conclusão). Esse estado deve ficar no domínio do
jogo, associado ao ID estável do objeto, e avançar em `Game::tick`. O
renderizador deve apenas consultar o clip e o tempo já escolhidos pelo jogo.
O formato atual aceita as cinco animações de locomoção do player e um clip por
objeto; ele precisará de novas chaves para clips adicionais. Não use um novo
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
