# Organização e extensão

A configuração da sala é um conjunto de dados. O estado do jogo, a entrada,
a renderização e as ferramentas de edição têm responsabilidades separadas.

| Local | Responsabilidade |
| --- | --- |
| `assets/rooms/*.room` | Salas editáveis por game design |
| `content/RoomParameters` | Catálogo de parâmetros: nome, descrição, intervalo, passo e acesso ao campo |
| `content/RoomFiles` | Leitura e gravação das salas; erros com número de linha |
| `content/RoomPresets` | Exemplo embutido usado por testes e por quem instancia `Game` diretamente |
| `game/Room` | Dados da sala, validação, escala, velocidade e limites |
| `game/SceneObject` | Identidade, desenho, tipo e base local de colisão |
| `game/Game` | Coordena entrada de gameplay, salas, navegação e jogador |
| `game/Player` | Posição atual/anterior e execução dos pontos da rota |
| `game/Navigation` | Grade, obstáculos, A* e simplificação segura das rotas |
| `debug/DebugEditor` | Reconhece DEBUG, processa comandos e gerencia edição/undo |
| `graphics/SceneRenderer` | Cenário e ordenação por profundidade |
| `graphics/DiagnosticsOverlay` | Textos técnicos de posição e viewport |
| `graphics/DebugOverlay` | Interface visual do editor |
| `graphics/Renderer` | Ciclo de desenho e apresentação lógica do SDL |
| `core/Application` | Janela, eventos SDL e atualização em passo fixo |

O executável e os testes compartilham `pointclick_core`. As rotinas de desenho
ficam em `pointclick_graphics`. Uma correção no jogo chega aos dois consumidores.

## Fluxos principais

Clique → conversão janela/mundo → `Game::onClick` → navegação → rota do `Player`.
A cada tick, o jogo calcula a velocidade na profundidade atual; o player avança
sem ultrapassar o próximo ponto. O render usa posições interpoladas.

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

Ao trocar os retângulos por sprites, preserve `bounds`, ID, base de colisão e
âncora dos pés. Adicione a referência do recurso à definição e trate o desenho
em `SceneRenderer`. A navegação não precisa conhecer texturas.

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
e conversão de coordenadas ao redimensionar. O teste gráfico usa o driver virtual
e o renderizador de software do SDL. As verificações também ficam ativas em Release.
