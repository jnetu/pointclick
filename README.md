# Point & Click

Protótipo em C++20 e SDL3: movimento por clique, perspectiva por profundidade,
objetos com bases sólidas, rotas com A*, sprites PNG e viewport lógica de
1280 × 720.

```sh
cmake --preset debug
cmake --build --preset debug
./build/debug/pointclick --rooms assets/rooms
```

O preset `debug` usa o gerador padrão da plataforma (ou `CMAKE_GENERATOR`);
com Ninja no `PATH`, use `ninja-debug`/`ninja-release`. O SDL3 do sistema só é
usado se for >= `POINTCLICK_SDL3_VERSION` (`cmake/Dependencies.cmake`); caso
contrário, o CPM baixa e compila essa mesma versão.
O SDL3_ttf (texto do jogo) segue a mesma regra com `POINTCLICK_SDL3_TTF_VERSION`; sem
pacote do sistema, o CPM o compila com o FreeType do sistema (ou com o FreeType fixado, em
mobile/console). As fontes ficam em [assets/fonts](assets/fonts), com as licenças OFL.

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

## Opções de linha de comando

| Opção | Efeito |
| --- | --- |
| `--rooms <pasta>` | Pasta das salas (padrão: `assets/rooms` junto do executável) |
| `--new-game` | Começa do zero, sem retomar o `save.slot` |
| `--no-save` | Não grava o autosave; o save existente fica intacto (ainda é lido, se não houver `--new-game`) |
| `--room <sala> [spawn]` | Começa um jogo **novo** em `<sala>.room`, no `[spawn]` indicado (sem ele: `player.start`). Não retoma o save; combine com `--no-save` para não sobrescrevê-lo |
| `--check` | Valida todas as salas, `.rules`, `.dlg`, o `player.actor` e os alvos de `change_room`, sem abrir janela. Só aceita `--rooms` |

```sh
./build/debug/pointclick --rooms assets/rooms --check
./build/debug/pointclick --rooms assets/rooms --no-save --room gallery
```

O `--check` imprime cada erro como `<arquivo>: Linha N: <motivo>` na saída de erro, um resumo
na saída padrão, e sai com `0` (tudo válido) ou `1` (algum erro); serve a CI. Opção inválida
sai com `2` e mostra o uso.

- [Guia para game design](docs/GAME_DESIGN.md): parâmetros, controles, colisões e novas salas.
- [Arquitetura e novas mecânicas](docs/ARCHITECTURE.md): responsabilidades e pontos de extensão.
- [Mapa rápido para código e IA](AGENTS.md): onde alterar cada comportamento.

```sh
ctest --test-dir build/debug --output-on-failure
```
