# Point & Click

Protótipo em C++20 e SDL3: movimento por clique, perspectiva por profundidade,
objetos com bases sólidas, rotas com A*, sprites PNG e viewport lógica de
1280 × 720.

```sh
cmake --preset debug
cmake --build --preset debug
./build/debug/pointclick --rooms assets/rooms
```

Digite **DEBUG** para abrir o editor durante o jogo. Use cima/baixo para
selecionar e esquerda/direita para alterar valores. **Tab** oculta o painel
para testar a sala; **Esc** sai do editor. Também há comandos:

```text
set player.far_speed 140
set player.near_speed 350
move red_box 850 460
save minha_sala.room
load gallery.room
```

Os arquivos de sala ficam em [assets/rooms](assets/rooms). O argumento `--rooms`
permite editar e salvar diretamente no projeto, sem recompilar. Alterações
feitas durante o jogo persistem após fechar somente quando se usa `save`.
Os PNGs ficam em [assets/sprites](assets/sprites). A sala define separadamente
o corte, o número de quadros e a velocidade de cada animação. Quando algum PNG
faltar ou tiver um corte inválido, ele aparece como um quadriculado roxo e preto.

- [Guia para game design](docs/GAME_DESIGN.md): parâmetros, controles, colisões e novas salas.
- [Arquitetura e novas mecânicas](docs/ARCHITECTURE.md): responsabilidades e pontos de extensão.
- [Mapa rápido para código e IA](AGENTS.md): onde alterar cada comportamento.

```sh
ctest --test-dir build/debug --output-on-failure
```
