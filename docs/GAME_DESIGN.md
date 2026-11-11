# Guia para game design

A câmera usa um campo de visão lógico fixo de **1280 × 720** em qualquer resolução
(720p, 1080p, 2K, 4K, Ultrawide, mobile ou console), preservando sempre o mesmo
enquadramento com letterbox. Por padrão, cada sala mede `1280 × 720` (câmera fixa);
salas panorâmicas ou mais altas podem aumentar `room.width` e `room.height`, e a
câmera acompanha suavemente os pés do personagem sem mostrar áreas fora da sala.
X cresce para a direita e Y cresce para baixo. A posição do personagem representa
o centro dos pés.

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

Com o painel oculto, texto e setas não alteram parâmetros; setas e Enter voltam a
valer para o jogo (por exemplo, o menu de escolhas de um diálogo). Os nomes dos objetos
e suas bases de colisão continuam visíveis. Fora do modo DEBUG, os comandos e
as setas não editam a sala.

A rota do personagem, a cruz no destino e o painel técnico de posições (canto superior
esquerdo) são desenhos de diagnóstico: só aparecem com o modo DEBUG ativo, e cada um pode
ser desligado com `show`. Com o editor fechado a tela mostra só o jogo e a dica
"DEBUG / F1 / Select" no canto. A versão do jogo sem ferramentas não desenha nada disso.

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
| `set room.width 2560` | Sala panorâmica com rolagem horizontal suave da câmera |
| `set room.walkable_y 320` | Limite superior para os pés |
| `teleport 640 500` | Reposiciona os pés em chão livre; não altera o ponto inicial salvo |
| `move red_box 850 460` | Move o objeto e sua base de colisão juntos |
| `size red_box 120 140` | Ajusta o tamanho visual; a base precisa caber nele |
| `base red_box 14 92 82 33` | Base local: X, Y, largura, altura |
| `solid red_box 0` | Desativa a colisão; `1` reativa |
| `save minha_sala.room` | Grava a configuração atual na pasta de salas |
| `load minha_sala.room` | Carrega a sala e usa sua posição inicial |
| `load minha_sala.room porta_esquerda` | Carrega a sala entrando pelo `[spawn porta_esquerda]` (teste de portas) |
| `show route 0` | Oculta a rota e a cruz do destino; `1` mostra de novo |
| `show diagnostics 0` | Oculta o painel técnico de posições |
| `reload` | Relê os PNGs do disco (arte trocada com o jogo aberto); não altera a sala nem o progresso |
| `show hint 0` | Oculta a dica "DEBUG / F1 / Select" exibida com o editor fechado (para capturas de tela limpas) |
| `undo` | Desfaz a última edição/carregamento; uma chamada seguinte alterna os dois estados |
| `reset` | Restaura a sala que estava ativa quando DEBUG foi aberto |

`save` substitui o arquivo informado e salva os parâmetros/objetos, não o
progresso do personagem. O arquivo é **regravado por inteiro** a partir da sala em
memória: comentários (`#`), linhas em branco e a ordem das chaves escritos à mão **são
descartados**. Guarde anotações de design fora do `.room` (ou salve com outro nome) se
for usar o `save`. Os números saem no texto mais curto que relê o mesmo valor (`300`,
`0.1`, `53.333332`), sempre com ponto decimal. Os arquivos `.rules` e `.dlg` não são
tocados pelo `save`. Para definir onde ele nasce, edite `player.start_x`
e `player.start_y`. Use ponto como separador decimal. Os nomes de arquivos
aceitam letras, números, `_` e `-`, com a extensão `.room`.

## Criar outra sala

Copie `assets/rooms/first.room`, dê outro nome ao arquivo e altere `id`.
O nome do arquivo é a identidade da sala: `biblioteca.room` precisa ter
`id = biblioteca` (ou omitir `id`, que então vale o nome do arquivo). É por esse
nome que `change_room` encontra a sala e que o progresso da partida é guardado;
um arquivo com `id` diferente do nome é recusado ao carregar.

### Pontos de entrada (`[spawn]`)

Uma sala com várias portas declara um bloco `[spawn <id>]` por entrada, com os pés
(`feet = x, y`, coordenadas da sala) e a direção para onde o jogador olha ao chegar
(`facing = left|right|up|down`, opcional, padrão `down`):

```ini
[spawn porta_esquerda]
feet = 120, 520
facing = right

[spawn porta_direita]
feet = 1150, 450
facing = left
```

A regra da porta de outra sala diz por qual entrada chegar:
`change_room biblioteca porta_esquerda`. Sem o segundo argumento
(`change_room biblioteca`) o jogador entra em `player.start_x`/`player.start_y`, olhando
para baixo, como antes. Um spawn fora da sala ou com ID repetido é recusado ao carregar;
um spawn sobre um obstáculo usa o ponto caminhável mais próximo. Se o spawn pedido não
existir na sala de destino, a troca inteira é recusada (log) e o jogador fica onde está; o
teste `testRoomSpawnFilesAndTransitions` confere todos os `change_room` dos assets
(`RoomLibrary::transitionsError`). O save guarda os pés e a direção, nunca o spawn usado,
então renomear ou mover spawns não quebra partidas salvas.
Edite os valores e use `load nome.room` no jogo, sem recompilar. Também é
possível salvar a sala atual com outro nome e continuar editando essa cópia;
o `save` grava o `id` do novo nome.
A pasta do editor aparece na parte inferior do painel.

O formato aceita comentários iniciados por `#`. Arquivos salvos em editores do Windows
(com BOM UTF-8 no início e quebras de linha CRLF) são lidos normalmente, em todos os
formatos (`.room`, `.rules`, `.dlg`, `.actor`). Chaves não reconhecidas,
valores inválidos e IDs repetidos são rejeitados. Os valores numéricos omitidos
usam os padrões de `Room.hpp`; a arte do jogador vem de `assets/actors/player.actor`
(veja abaixo). Nenhum dado de arte vem do C++. Os objetos devem ser declarados no arquivo.

## Sprites e animações

Coloque os PNGs em `assets/sprites`. Cada `*.file` é relativo a essa pasta;
subpastas também funcionam, por exemplo `personagens/player_idle.png`. O jogo
procura os sprites ao carregar a sala. Se o arquivo faltar ou o corte não couber
na imagem, ele desenha um quadriculado roxo e preto no lugar.

### Arte do jogador (`assets/actors/player.actor`)

Os clips do jogador ficam **uma vez só** em `assets/actors/player.actor`, e todas as salas
os herdam. Cada direção tem seu conjunto de valores: `idle`, `left`, `right`, `up` e
`down`. Esses cinco nomes são usados pelo movimento atual; `idle` é usado quando o
jogador para. Clips opcionais `idle_left`, `idle_right`, `idle_up` e `idle_down` substituem
o `idle` quando o jogador está parado olhando nessa direção (a do último passo ou a do
`[spawn]` de chegada); sem eles, vale o `idle`. Outras animações podem ser cadastradas com `NOME.file`, `NOME.frames` e os
mesmos sufixos abaixo, sem alterar o leitor. A regra que aciona uma nova animação ainda
precisa ser implementada no jogo.

Uma sala pode sobrescrever só o que muda, no bloco `[room]`, com o prefixo `player.`:
`player.idle.fps = 4` troca apenas o fps do `idle` herdado, e `player.NOME.*` acrescenta um
clip só daquela sala. Ao salvar pelo editor, os clips iguais aos do ator ficam fora do
`.room`. Uma sala lida sem o ator (por exemplo, numa pasta sem `actors/`) precisa declarar
os cinco clips.

Sufixos de um clip (no `.actor` sem prefixo, na sala com `player.` ou `animation.`):

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
| `.flip_x` | `1` espelha o clip na horizontal (ex.: `left` reaproveitando a folha de `right`); padrão `0`. Só é gravado quando vale `1`, então arquivos antigos salvam iguais |
| `.display_width`, `.display_height` | Tamanho visual base; 0 usa `player.width/height` ou `bounds` do objeto |
| `.offset_x`, `.offset_y` | Ajuste visual da imagem em relação ao centro dos pés, antes da perspectiva |

Os quadros são lidos da esquerda para a direita, depois de cima para baixo.
Para uma imagem simples, use `.frames = 1` e deixe `.frame_width` e
`.frame_height` em 0. Para uma spritesheet, defina os dois tamanhos e a
quantidade de quadros. O `player.actor` atual usa estes cortes reais:

```ini
idle.file = player_idle.png
idle.frame_width = 32
idle.frame_height = 48
idle.frames = 8
idle.columns = 1
idle.fps = 6

left.file = player_esquerda.png
left.frame_width = 32
left.frame_height = 48
left.frames = 7
left.columns = 7
left.spacing_x = 1
left.fps = 8

# right, up e down seguem o mesmo corte de left com player_direita.png,
# player_cima.png e player_baixo.png.
```

O tamanho visual e os deslocamentos são medidos nas coordenadas da sala. A
perspectiva da sala é aplicada depois: o sprite diminui ao se afastar da câmera
e continua ancorado nos pés usados para caminhar e ordenar as camadas.
Para manter a proporção original de cada quadro, ajuste `.display_width` e
`.display_height` na mesma proporção. A primeira sala usa largura visual
53,33 com altura 80, preservando a proporção 32:48 de cada quadro. Após editar
o `.room` ou colocar um PNG na pasta, use
`load first.room` no modo DEBUG para recarregar sem recompilar: ele relê a sala e os
PNGs. Se só a arte mudou, `reload` relê os PNGs sem mexer na sala, na posição do
personagem nem no progresso.

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

O jogo guarda o tempo de animação de cada objeto separadamente. Uma regra
declarativa pode usar `play_animation porta open` (ou em C++ chamar
`Game::playObjectAnimation("porta", "open", true, error)`) para iniciar ou
reiniciar a ação. O clip com `loop = 0` permanece no último quadro;
`AnimationPlayback::finished(clip)` informa quando terminou (um clip com `fps = 0` e
`loop = 0` é uma imagem parada e conta como terminado desde o início). `bounds` continua
definindo a posição, o hit-test de clique e o tamanho visual padrão; `collision`
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
tem um ID estável, como `mesa`, usado nos comandos e nas regras de interação.

## Parâmetros e relações

| Chave | O que controla |
| --- | --- |
| `room.width`, `room.height` | Largura e altura totais da sala (padrão e mínimo `1280 × 720`; salas maiores ativam rolagem suave da câmera) |
| `room.wall_y` | Divisão visual entre parede e chão |
| `room.walkable_y` | Menor Y permitido para os pés; deve ser igual ou maior que `room.wall_y` |
| `perspective.far_y`, `perspective.near_y` | Limites de profundidade; o primeiro deve ser menor que o segundo e caber em `room.height` |
| `perspective.far_scale`, `perspective.near_scale` | Escala visual nas duas profundidades |
| `player.far_speed`, `player.near_speed` | Velocidades nas mesmas profundidades; valores iguais dão velocidade constante |
| `player.width`, `player.height` | Tamanho visual base do jogador |
| `player.start_x`, `player.start_y` | Ponto inicial ao carregar a sala (dentro de `room.width × room.height`) |
| `collision.padding_x`, `collision.padding_y` | Margens extras em volta da base dos objetos |
| `room.vanishing_x`, `room.ray_spacing` | Ponto de fuga (entre `0` e `room.width`) e espaçamento das linhas do chão |
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

## Objetos clicáveis, itens e puzzles (`<sala>.rules`)

Para manter o arquivo `.room` limpo (focado apenas em geometria, cores, colisões
base e sprites), a lógica interativa da cena fica em um arquivo companheiro
**opcional** com o mesmo nome e extensão `.rules` na pasta `assets/rooms/` — por
exemplo, `first.rules` ao lado de `first.room`, ou `gallery.rules` ao lado de
`gallery.room`.

Quando o jogador clica em um objeto visível que possui regras:
1. Se dois objetos interativos se sobrepõem, o jogo escolhe automaticamente o que
   está **na frente** pela profundidade visual (`bounds.y + bounds.h`).
2. O personagem caminha até o ponto alcançável mais próximo da base do objeto
   (*Walk-to-Interact*). Clicar no chão vazio durante o trajeto cancela a ação.
3. Ao chegar (ou imediatamente, se já estiver parado junto ao objeto), as regras
   daquele objeto são avaliadas **de cima para baixo**: a **primeira** regra cujas
   condições forem todas verdadeiras executa seus efeitos em ordem.

### Formato do arquivo `.rules`

Comentários começam com `#` (fora de aspas). Cada bloco de regra inicia com um
gatilho entre colchetes, seguido por zero ou mais condições e um ou mais efeitos:

| Elemento | Sintaxe | Descrição |
| --- | --- | --- |
| **Gatilho** | `[interact <objectId>]` | Clique direto no objeto da sala |
| **Gatilho** | `[use_item <itemId> <objectId>]` | Uso de um item do inventário sobre o objeto |
| **Gatilho curinga** | `[interact *]` | Clique em qualquer objeto da sala sem regra `interact` que se aplique (torna todo objeto visível clicável) |
| **Gatilho curinga** | `[use_item * <objectId>]` / `[use_item <itemId> *]` / `[use_item * *]` | Uso de qualquer item no objeto / do item em qualquer objeto / de qualquer item em qualquer objeto, quando nenhuma regra mais específica se aplica |
| **Condição** | `flag_set <flag>` / `flag_not_set <flag>` | Testa se uma flag de progresso está ativa ou não |
| **Condição** | `flag_at_least <flag> <n>` / `flag_equals <flag> <n>` | Compara o contador da flag com o inteiro `<n>` (flag ausente vale `0`; `set_flag` vale `1`) |
| **Condição** | `has_item <itemId>` / `lacks_item <itemId>` | Testa se o jogador possui (ou não) o item no inventário |
| **Condição** | `object_visible <objectId>` / `object_hidden <objectId>` | Testa se um objeto da sala está visível ou oculto |
| **Efeito** | `say "<texto>"` | Exibe a fala/observação e espera o tempo de leitura ou um clique. Texto UTF-8 com acentos. Aceita `\"` e `\n` (quebra de linha forçada); o resto quebra sozinho por palavras (até ~820 px de largura), centralizado acima da cabeça do player e sempre dentro da tela |
| **Efeito** | `set_flag <flag>` / `clear_flag <flag>` | Ativa ou limpa uma flag na sessão |
| **Efeito** | `add_flag <flag> <n>` | Soma o inteiro `<n>` ao contador da flag (`-1` decrementa); chegar a `0` limpa a flag |
| **Efeito** | `give_item <itemId>` / `take_item <itemId>` | Adiciona ou remove um item do inventário |
| **Efeito** | `hide_object <objectId>` / `show_object <objectId>` | Esconde (ex: item pego) ou revela um objeto na sala |
| **Efeito** | `set_solid <objectId> <0\|1>` | Desativa (`0`) ou ativa (`1`) a colisão e reconstrói a navegação |
| **Efeito** | `play_animation <objectId> <animName>` | Toca/reinicia um clip configurado no objeto |
| **Efeito** | `wait_animation <objectId>` | Espera o clip atual do objeto terminar (clips com `loop` e imagens paradas, com `fps = 0`, não esperam) |
| **Efeito** | `walk_to <objectId>` | Anda até o ponto de aproximação do objeto e espera chegar |
| **Efeito** | `face <left\|right\|up\|down>` | Vira o player parado para a direção (as mesmas de `PlayerPose`/`[spawn]`); parado, usa `idle_<direção>` se existir. Outra palavra é recusada com a linha (`Direcao invalida: ...`) |
| **Efeito** | `wait <segundos>` | Espera um tempo (ex.: `wait 1.5`, sempre com ponto) |
| **Efeito** | `start_dialogue <no>` | Abre um nó do `<sala>.dlg` e espera a conversa terminar |
| **Efeito** | `change_room <roomId> [<spawn>]` | Troca para `<roomId>.room` quando a sequência terminar; entra pelo `[spawn]` indicado (sem ele: `player.start_*`) |

A tabela acima espelha o catálogo de ações em `src/game/ActionCatalog.cpp`. Os
argumentos são validados pelo tipo: identificadores só aceitam letras, números, `_`
e `-`; `<objeto>` precisa existir na sala; `<animacao>` precisa existir no objeto;
`"texto"` não pode ser vazio; booleanos aceitam só `0` ou `1`; `<n>` é um inteiro com `-`
opcional (sem `+`, sem fração), de `-1000000` a `1000000`. Argumentos entre colchetes
na tabela são opcionais e só podem faltar no fim. Um erro aponta a linha
e a forma esperada (por exemplo, `Linha 4: Use set_solid <objeto> <0|1>`).

### Regras curinga e resposta padrão

`*` no lugar do objeto ou do item vale para qualquer um. Serve para a resposta padrão, sem
escrever uma regra por combinação:

```ini
[use_item brass_key red_box]   # a combinação certa
take_item brass_key
say "Abriu!"

[use_item * red_box]           # qualquer outro item na caixa
say "A caixa não abre com isso."

[use_item * *]                 # qualquer item em qualquer objeto da sala
say "Isso não funciona."

[interact *]                   # clique em objeto sem regra própria
say "Nada de interessante."
```

A escolha da regra é determinística e vai **da mais específica para a mais ampla**, nesta
ordem fixa:

1. exata: `[interact <obj>]` ou `[use_item <item> <obj>]`;
2. `[interact *]` e `[use_item * <obj>]` (a resposta do próprio objeto);
3. `[use_item <item> *]` (a resposta do item em qualquer objeto);
4. `[use_item * *]`.

Dentro de um mesmo nível vale a ordem do arquivo (a primeira regra cujas condições passarem).
A posição no arquivo **não** muda o nível: uma curinga escrita antes da regra exata continua
perdendo para ela. Se as condições de todas as regras de um nível falharem, a avaliação desce
para o próximo; por isso `[use_item * porta]` também responde quando a regra exata da chave
já não vale (por exemplo, com a porta aberta). Sem nenhuma curinga, um item sem regra continua
sem resposta, como antes. Um gatilho com `*` no lugar do objeto (`[interact *]`,
`[use_item <item> *]`, `[use_item * *]`) conta como regra de todos os objetos: com ele, todo
objeto visível da sala passa a ser clicável.

### Sequências e cutscenes

Os efeitos rodam **em ordem e ao longo do tempo**. Efeitos instantâneos (flags, itens,
visibilidade, colisão, `play_animation`, `change_room`) acontecem na hora; `say`,
`walk_to`, `wait` e `wait_animation` seguram a sequência até terminar. Enquanto isso o
jogo fica em **modo cutscene**: clicar no cenário não move o personagem nem dispara
outras regras, só pula a fala atual. O último efeito nunca é esperado, então uma regra
que termina em `say` devolve o controle na hora. `change_room` só acontece quando a
sequência inteira termina.

```ini
# Puxar a alavanca: animar -> esperar -> andar ate a porta -> falar -> trocar de sala
[interact alavanca]
play_animation porta open
wait_animation porta
wait 0.5
walk_to porta
say "Vamos la."
change_room gallery
```

Falas lineares de diálogo são só uma sequência de `say`; cada clique avança para a
próxima.

### Diálogos com escolhas (`<sala>.dlg`)

Conversas com escolhas ficam num terceiro arquivo companheiro **opcional**, com o mesmo nome
da sala e extensão `.dlg` (por exemplo, `gallery.dlg` ao lado de `gallery.room` e
`gallery.rules`). Uma regra abre a conversa com `start_dialogue <no>`:

```ini
# gallery.rules
[interact table]
start_dialogue mesa
```

O `.dlg` tem dois tipos de bloco. `[node <id>]` abre um nó; as linhas logo abaixo são
efeitos do catálogo (`say`, `play_animation`, `wait`...) que rodam quando o nó abre.
`[choice <id> "texto"]` é uma opção do nó acima dela, mostrada no menu com esse texto. Dentro
de uma escolha, nesta ordem:

| Linha | Descrição |
| --- | --- |
| condições (`has_item`, `flag_set`...) | Mesmas condições do `.rules`; a opção só aparece se todas valerem |
| `once` | A opção some depois de escolhida uma vez (fica salvo no progresso da partida) |
| efeitos (`say`, `give_item`, `set_flag`, `change_room`...) | Rodam em sequência quando a opção é escolhida |
| `goto <no>` | Última linha, opcional: abre outro nó (ou o mesmo, para voltar ao menu). Sem `goto`, a conversa termina |

```ini
# gallery.dlg
[node mesa]
say "Uma mesa de madeira sólida. O que fazer?"

# So aparece depois de achar o bilhete na planta.
[choice ler_bilhete "Ler o bilhete sobre a mesa."]
has_item gallery_note
say "Coloquei o bilhete sobre a mesa para ler melhor."
set_flag read_gallery_note

[choice gavetas "Procurar nas gavetas."]
once
say "Gavetas vazias... mas uma delas tem fundo falso."
goto fundo_falso

[choice sair "Deixar a mesa."]
say "Melhor seguir em frente."

[node fundo_falso]
say "Forçar o fundo falso?"

[choice forcar "Forçar."]
say "Nada além de poeira."
goto mesa

[choice deixar "Deixar quieto."]
```

Como se joga: as falas do nó aparecem e, junto com a última, o menu com as opções disponíveis
(na ordem do arquivo) na parte de baixo da tela. Escolha clicando ou tocando na linha, com
as setas/D-pad + Enter/Start, ou com o botão A do controle (com o cursor sobre a linha, ou
depois de marcá-la com o D-pad). Clicar
fora das opções não faz nada. A resposta da opção roda como uma sequência (o clique pula a
fala); quando há `goto`, a última fala da resposta é esperada antes do próximo nó abrir. Se
nenhuma opção estiver disponível, a conversa termina depois das falas do nó. Ao terminar, a
regra que chamou `start_dialogue` continua do passo seguinte; `change_room` dentro da
conversa só acontece quando tudo terminar.

Regras do formato: IDs de nó são únicos no arquivo e IDs de escolha são únicos no nó (letras,
números, `_` e `-`); o texto da opção não pode ser vazio; todo `goto` aponta para um nó do
arquivo; todo `start_dialogue` do `.rules` aponta para um nó do `.dlg`; `start_dialogue` não
vale dentro do `.dlg` (use `goto`); condições, `once` e `goto` só valem dentro de
`[choice ...]`. Um erro aponta a linha (por exemplo,
`Linha 12: Destino de goto desconhecido: porao`). O comando `load` do painel DEBUG recarrega
`.room`, `.rules` e `.dlg` juntos. Falas e opções aceitam acentos (UTF-8); uma opção longa
quebra em várias linhas e a linha do menu cresce para caber.

### Texto na tela e fontes

O texto do jogo usa fontes TrueType de `assets/fonts/` (licença SIL OFL, `OFL.txt` ao lado de
cada família; mantenha-o junto ao distribuir). O código pede um **papel**, não um arquivo; o
mapeamento fica só em `graphics/FontLibrary.cpp`:

| Papel | Fonte | Uso |
| --- | --- | --- |
| `dialogue` | Atkinson Hyperlegible Regular, 30 px | Falas e observações (`say`), com contorno escuro |
| `choice` | Atkinson Hyperlegible Regular, 24 px | Opções do menu de diálogo |
| `speaker` | Lora Regular, 26 px | Nome de quem fala, acima da fala (pronto; `say` ainda não tem orador) |
| `title` | Lora Regular, 44 px | Títulos (reservado para menus futuros) |

Tamanhos em pixels da viewport `1280×720`; a fonte é rasterizada na escala real da tela, então
fica nítida em telas de alta densidade. Itálico e negrito das duas famílias já estão na pasta para
ênfase futura. Se uma fonte faltar, o jogo registra no log e usa a fonte de depuração (sem
acentos) só para aquele papel.

### Exemplo prático: chave coletável, NPC condicional e porta trancada

```ini
# Coletar item: desaparece da cena e entra no inventario
[interact chave_antiga]
object_visible chave_antiga
give_item chave_antiga
hide_object chave_antiga
say "Peguei uma chave antiga no chao."

# Porta ja aberta: atravessa para outra sala
[interact porta]
flag_set porta_aberta
change_room gallery

# Porta trancada, mas jogador ja tem a chave: destranca, anima e libera colisao
[interact porta]
has_item chave_antiga
take_item chave_antiga
set_flag porta_aberta
set_solid porta 0
play_animation porta open
say "A chave girou! A passagem esta livre."

# Fallback quando ainda nao tem a chave
[interact porta]
say "Esta trancada."
```

### Edição ao vivo sem perder o progresso (`SessionState`)

Todo o progresso da partida (flags, itens no inventário, objetos escondidos e
portas destrancadas) fica salvo em `SessionState`, separado da definição `Room`.
Isso significa que você pode abrir o modo **DEBUG**, ajustar velocidades (`set`),
mover objetos (`move`), desfazer ajustes visuais (`undo`) ou trocar de sala sem
perder os itens coletados nem resetar puzzles já resolvidos. Caso edite o arquivo
`.rules` no disco, basta executar `load nome_da_sala.room` no painel DEBUG para
revalidar (com indicação do número da linha em caso de erro) e recarregar as regras.
