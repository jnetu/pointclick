# Guia para game design

O jogo usa uma tela lógica de **1280 × 720**. X cresce para a direita e Y cresce
para baixo. A posição do personagem representa o centro dos pés. Redimensionar
a janela preserva essas coordenadas e a proporção do cenário.

## Abrir o jogo para trabalhar nas salas

A partir da raiz do projeto:

```sh
cmake --preset debug
cmake --build --preset debug
./build/debug/pointclick --rooms assets/rooms
```

O argumento `--rooms` define a pasta usada para carregar e salvar. Assim as
edições ficam nos arquivos do projeto. No CLion, coloque `--rooms assets/rooms`
nos argumentos e use a raiz do projeto como diretório de trabalho.
Sem esse argumento, o jogo usa a cópia de `assets/rooms` junto do executável;
essa cópia é atualizada durante o build.

A primeira sala carregada é `first.room`. Existe também `gallery.room`, uma
segunda sala de exemplo com parâmetros e objetos diferentes.

## Editar durante o jogo

Digite **DEBUG** enquanto a janela está em foco. Não precisa apertar Enter
para abrir o editor. Maiúsculas e minúsculas funcionam.

- **Cima/baixo:** selecionar um parâmetro da lista.
- **Esquerda/direita:** diminuir/aumentar pelo passo indicado.
- **Enter:** executar o comando digitado na linha inferior.
- **Backspace:** apagar o último caractere do comando.
- **Tab:** ocultar/mostrar o painel para testar a sala inteira com o mouse.
- **Esc:** sair do modo debug; os valores editados continuam na sessão.

Com o painel oculto, texto e setas não alteram parâmetros. Os nomes dos objetos
e suas bases de colisão continuam visíveis. Fora do modo DEBUG, os comandos e
as setas não editam a sala. O painel técnico de posições continua disponível.

As mudanças válidas são aplicadas imediatamente. Elas param a rota antiga e
reconstroem a navegação. Se o jogador ficar dentro de um obstáculo após a edição,
ele é colocado no chão livre mais próximo. Mudanças inválidas mostram uma
mensagem e preservam o estado anterior.

| Comando | Resultado |
| --- | --- |
| `set player.far_speed 140` | Velocidade no fundo: 140 pixels lógicos/segundo |
| `set player.near_speed 350` | Velocidade perto da câmera |
| `set player.width 50` | Largura antes de aplicar a escala |
| `set perspective.far_scale 0.6` | Escala no fundo |
| `set room.walkable_y 320` | Limite superior para os pés |
| `teleport 640 500` | Reposiciona os pés em chão livre; não altera o ponto inicial salvo |
| `move red_box 850 460` | Move o objeto e sua base de colisão juntos |
| `size red_box 120 140` | Ajusta o tamanho visual; a base precisa caber nele |
| `base red_box 14 92 82 33` | Base local: X, Y, largura, altura |
| `solid red_box 0` | Desativa a colisão; `1` reativa |
| `save minha_sala.room` | Grava a configuração atual na pasta de salas |
| `load minha_sala.room` | Carrega a sala e usa sua posição inicial |
| `undo` | Desfaz a última edição/carregamento; uma chamada seguinte alterna os dois estados |
| `reset` | Restaura a sala que estava ativa quando DEBUG foi aberto |

`save` substitui o arquivo informado e salva os parâmetros/objetos, não o
progresso do personagem. Para definir onde ele nasce, edite `player.start_x`
e `player.start_y`. Use ponto como separador decimal. Os nomes de arquivos
aceitam letras, números, `_` e `-`, com a extensão `.room`.

## Criar outra sala

Copie `assets/rooms/first.room`, dê outro nome ao arquivo e altere `id`.
Edite os valores e use `load nome.room` no jogo, sem recompilar. Também é
possível salvar a sala atual com outro nome e continuar editando essa cópia.
A pasta do editor aparece na parte inferior do painel.

O formato aceita comentários iniciados por `#`. Chaves não reconhecidas,
valores inválidos e IDs repetidos são rejeitados. Os valores numéricos omitidos
usam os padrões de `Room.hpp`; os sprites padrão do jogador ficam em
`src/content/RoomPresets.cpp`. Os objetos devem ser declarados no arquivo.

## Sprites e animações

Coloque os PNGs em `assets/sprites`. Cada `*.file` é relativo a essa pasta;
subpastas também funcionam, por exemplo `personagens/player_idle.png`. O jogo
procura os sprites ao carregar a sala. Se o arquivo faltar ou o corte não couber
na imagem, ele desenha um quadriculado roxo e preto no lugar.

Cada direção do jogador tem seu próprio conjunto de valores: `player.idle`,
`player.left`, `player.right`, `player.up` e `player.down`. Esses cinco nomes
são usados pelo movimento atual. Outras animações podem ser cadastradas com
`player.NOME.file`, `player.NOME.frames` e os mesmos sufixos abaixo, sem alterar
o leitor de salas. A regra que aciona uma nova animação ainda precisa ser
implementada no jogo. A animação `idle` é usada quando o jogador para.
Edite as chaves abaixo no bloco `[room]` de cada sala:

| Sufixo | Significado |
| --- | --- |
| `.file` | Caminho relativo do PNG |
| `.frame_width`, `.frame_height` | Tamanho, em pixels do PNG, de **um** quadro; ambos 0 usam a imagem inteira |
| `.frames` | Número de quadros da animação |
| `.columns` | Quadros por linha; 0 calcula pela largura da imagem |
| `.first_frame` | Quantos espaços da grade pular antes do primeiro quadro |
| `.margin_x`, `.margin_y` | Margem antes da grade de quadros, em pixels |
| `.spacing_x`, `.spacing_y` | Espaço entre quadros, em pixels |
| `.fps` | Quadros por segundo; 0 mantém o primeiro quadro |
| `.loop` | `1` repete a animação; `0` para no último quadro |
| `.pixelated` | `1` usa pixels nítidos; `0` suaviza a ampliação |
| `.display_width`, `.display_height` | Tamanho visual base; 0 usa `player.width/height` ou `bounds` do objeto |
| `.offset_x`, `.offset_y` | Ajuste visual da imagem em relação ao centro dos pés, antes da perspectiva |

Os quadros são lidos da esquerda para a direita, depois de cima para baixo.
Para uma imagem simples, use `.frames = 1` e deixe `.frame_width` e
`.frame_height` em 0. Para uma spritesheet, defina os dois tamanhos e a
quantidade de quadros. As outras salas usam os mesmos cortes padrão quando não
definem suas próprias chaves `player.*`. A primeira sala usa estes cortes reais:

```ini
player.idle.file = player_idle.png
player.idle.frame_width = 32
player.idle.frame_height = 48
player.idle.frames = 8
player.idle.columns = 1
player.idle.fps = 6

player.left.file = player_esquerda.png
player.left.frame_width = 32
player.left.frame_height = 48
player.left.frames = 7
player.left.columns = 7
player.left.spacing_x = 1
player.left.fps = 8

player.right.file = player_direita.png
player.right.frame_width = 32
player.right.frame_height = 48
player.right.frames = 7
player.right.columns = 7
player.right.spacing_x = 1
player.right.fps = 8

player.up.file = player_cima.png
player.up.frame_width = 32
player.up.frame_height = 48
player.up.frames = 7
player.up.columns = 7
player.up.spacing_x = 1
player.up.fps = 8

player.down.file = player_baixo.png
player.down.frame_width = 32
player.down.frame_height = 48
player.down.frames = 7
player.down.columns = 7
player.down.spacing_x = 1
player.down.fps = 8
```

O tamanho visual e os deslocamentos são medidos nas coordenadas da sala. A
perspectiva da sala é aplicada depois: o sprite diminui ao se afastar da câmera
e continua ancorado nos pés usados para caminhar e ordenar as camadas.
Para manter a proporção original de cada quadro, ajuste `.display_width` e
`.display_height` na mesma proporção. A primeira sala usa largura visual
53,33 com altura 80, preservando a proporção 32:48 de cada quadro. Após editar
o `.room` ou colocar um PNG na pasta, use
`load first.room` no modo DEBUG para recarregar sem recompilar.

Objetos também aceitam clips nomeados. Dentro de `[object mesa]`, use
`animation.idle.file = cenario/mesa.png` para uma imagem simples e acrescente
`animation.open.file`, `animation.open.frames` e outros campos para uma ação.
`animation.initial` define qual clip começa ativo. Os mesmos sufixos da tabela
funcionam para objetos. O antigo `sprite.file` continua aceito como sinônimo
de `animation.idle.file` ao carregar salas antigas. Exemplo:

```ini
[object porta]
bounds = 800, 330, 110, 260
type = solid
collision = 10, 220, 90, 40
animation.initial = idle
animation.idle.file = cenario/porta_fechada.png
animation.open.file = cenario/porta_abrindo.png
animation.open.frame_width = 64
animation.open.frame_height = 128
animation.open.frames = 6
animation.open.fps = 8
animation.open.loop = 0
```

O jogo guarda o tempo de animação de cada objeto separadamente. Uma regra C++
pode chamar `Game::playObjectAnimation("porta", "open", true, error)` para
iniciar ou reiniciar a ação. O clip com `loop = 0` permanece no último quadro;
`AnimationPlayback::finished(clip)` informa quando terminou. O clique para
abrir a porta e as condições dessa interação ainda precisam ser implementados.
`bounds` continua definindo a posição e o tamanho visual padrão; `collision`
continua definindo a área sólida. O sprite não altera a navegação.

```ini
[room]
id = biblioteca
room.wall_y = 260
room.walkable_y = 290
perspective.far_y = 290
perspective.near_y = 720
perspective.far_scale = 0.6
perspective.near_scale = 1.5
player.start_x = 300
player.start_y = 440
player.far_speed = 120
player.near_speed = 300
wall.color = 100, 75, 60, 255
floor.color = 65, 100, 145, 255

[object mesa]
bounds = 700, 370, 180, 150
color = 150, 100, 70, 255
type = solid
collision = 10, 120, 160, 30
```

`bounds` é o retângulo visual no mundo. `collision` é a área sólida **relativa
ao canto superior esquerdo do objeto**. A base da mesa acima ocupa X=710,
Y=490, largura=160 e altura=30. A parte superior continua visual: o personagem
pode aparecer atrás dela. `type = decoration` desativa a colisão. Cada objeto
tem um ID estável, como `mesa`, usado nos comandos e em futuras regras de interação.

## Parâmetros e relações

| Chave | O que controla |
| --- | --- |
| `room.wall_y` | Divisão visual entre parede e chão |
| `room.walkable_y` | Menor Y permitido para os pés; deve ser igual ou maior que `room.wall_y` |
| `perspective.far_y`, `perspective.near_y` | Limites de profundidade; o primeiro deve ser menor que o segundo |
| `perspective.far_scale`, `perspective.near_scale` | Escala visual nas duas profundidades |
| `player.far_speed`, `player.near_speed` | Velocidades nas mesmas profundidades; valores iguais dão velocidade constante |
| `player.width`, `player.height` | Tamanho visual base do jogador |
| `player.start_x`, `player.start_y` | Ponto inicial ao carregar a sala |
| `collision.padding_x`, `collision.padding_y` | Margens extras em volta da base dos objetos |
| `room.vanishing_x`, `room.ray_spacing` | Ponto de fuga e espaçamento das linhas do chão |
| `floor.lines` | Quatro frações entre 0 e 1 para as linhas horizontais de perspectiva |

As cores são RGBA, com quatro inteiros entre 0 e 255. As chaves são
`wall.color`, `floor.color`, `wall.trim_color`, `floor.guide_color`,
`player.color` e `player.outline_color`. Para editar cores ou linhas, altere
o arquivo e use `load` novamente. Use alfa 255 para os retângulos atuais.
As duas cores do jogador continuam aceitas nos arquivos antigos, mas não
afetam o jogador desenhado com PNG.

Tamanho e velocidade são interpolados linearmente pela altura dos pés.
A profundidade de desenho usa o Y dos pés do jogador e a borda inferior dos
objetos: valores maiores aparecem na frente. Colisão e desenho são independentes.

A navegação usa células de 32 pixels. A área proibida inclui a base, as margens
e as células que elas tocam; por isso o afastamento pode ser maior que o
retângulo desenhado. Se mudar o tamanho do player, confira também as margens.
Um clique na área bloqueada procura o ponto de grade alcançável mais próximo.

## Novas regras de jogo

Os arquivos já permitem definir as regras de deslocamento, perspectiva,
colisão e composição de cada sala. Para novos comportamentos — conversar,
pegar itens, abrir portas, testar condições de quests — consulte
[ARCHITECTURE.md](ARCHITECTURE.md). Esses comportamentos ainda exigem C++;
não há uma linguagem de scripts de gameplay no formato `.room`.
