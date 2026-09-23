# Point & Click

Protótipo em SDL3. O jogador começa no centro do mundo e caminha até o ponto
clicado com o botão esquerdo. Um novo clique substitui a rota atual. Cliques
nas barras fora da área do jogo são ignorados.

```sh
cmake --preset debug
cmake --build --preset debug
./build/debug/pointclick
```

O mundo usa coordenadas lógicas de 1280 × 720. A apresentação do SDL preserva
essa proporção ao redimensionar a janela, e a posição do mouse é convertida de
coordenadas da janela para coordenadas do mundo. Os textos de debug usam âncoras
nos cantos do mundo.

Os parâmetros da sala ficam em [`src/game/Room.hpp`](src/game/Room.hpp):
`wallBottomY` separa parede vermelha e chão azul, `walkableTopY` limita onde os
pés podem ficar, e `farDepthY`/`nearDepthY` com `farScale`/`nearScale` controlam
o tamanho aparente do jogador. Cores, linhas de perspectiva, tamanho base,
velocidade, ponto inicial e objetos da cena também podem ser ajustados. Os
valores da primeira sala ficam em `Room::firstRoom()`; outra sala pode ser
passada para `Game` ou ativada com `Game::loadRoom()`.

`Navigation` guarda uma grade de células caminháveis e calcula rotas com A*.
Quando o caminho direto está livre, o destino é usado sem desvios. O objeto
vermelho à direita usa `SceneObjectType::solid`: somente sua base definida em
`collisionFootprint` bloqueia os pés do jogador. A parte superior continua
visual e pode cobrir o jogador conforme a profundidade. Um clique na base
bloqueada é levado ao ponto alcançável mais próximo. Outros objetos sólidos
podem ser adicionados a `Room::scenery` com sua própria base e a margem de
colisão ajustada em `Room`.

Para rodar os testes:

```sh
ctest --test-dir build/debug --output-on-failure
```
