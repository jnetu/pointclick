# Point & Click

Engine de jogos point & click 2D em C++20 e SDL3, com um playground de salas de teste:
movimento por clique com rotas A*, perspectiva por profundidade, objetos com bases sólidas,
sprites PNG animados, regras de interação e diálogos declarativos (`.rules`, `.dlg`), texto
UTF-8 com SDL3_ttf, autosave e viewport lógica de 1280 × 720. O alvo é PC, celular e tablet;
hoje só o desktop foi testado (o estado e as pendências estão em
[docs/ROADMAP.md](docs/ROADMAP.md)).

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
| `--no-save` | Não grava o autosave; o save existente fica intacto (ainda é lido, se não houver `--new-game`). Um save recusado (corrompido ou de versão desconhecida) também fica onde está, sem virar `save.slot.bad`: o jogo avisa no log e começa do zero |
| `--room <sala> [spawn]` | Começa um jogo **novo** em `<sala>.room`, no `[spawn]` indicado (sem ele: `player.start`). Não retoma o save; combine com `--no-save` para não sobrescrevê-lo |
| `--check` | Valida todas as salas, `.rules`, `.dlg`, o `player.actor` e os alvos de `change_room`, sem abrir janela. Arte ausente (PNG, corte que não cabe na imagem, fonte) sai como aviso e não muda o código de saída. Só aceita `--rooms` |

```sh
./build/debug/pointclick --rooms assets/rooms --check
./build/debug/pointclick --rooms assets/rooms --no-save --room gallery
```

O `--check` imprime cada erro como `<arquivo>: Linha N: <motivo>` na saída de erro, um resumo
na saída padrão, e sai com `0` (tudo válido) ou `1` (algum erro); serve a CI. Opção inválida
sai com `2` e mostra o uso.

Arte que o jogo consegue substituir não é erro: PNG ausente ou ilegível, corte de spritesheet
que não cabe na imagem, clip sem arquivo e fonte ausente saem como
`aviso: <sala>: <arquivo>: <motivo>` na saída de erro, entram na contagem do resumo
(`N sala(s) verificada(s), N erro(s), N aviso(s)`) e **não** mudam o código de saída. O jogo
roda do mesmo jeito (quadriculado roxo no lugar do sprite, fonte de debug no lugar da TTF) e
registra os mesmos avisos no log ao carregar cada sala, uma vez por execução.

- [Guia para game design](docs/GAME_DESIGN.md): parâmetros, controles, colisões e novas salas.
- [Arquitetura e novas mecânicas](docs/ARCHITECTURE.md): responsabilidades e pontos de extensão.
- [Mapa rápido para código e IA](AGENTS.md): onde alterar cada comportamento.

```sh
ctest --test-dir build/debug --output-on-failure
```

## Formatação

O [.clang-format](.clang-format) foi calibrado com o **clang-format 21** (versão major fixada);
outro major pode formatar diferente e gerar ruído de diff. A mesma versão em qualquer sistema:

```sh
pip install "clang-format==21.*"
```

Funciona igual no macOS, Linux e Windows (use um venv ou `pipx install "clang-format==21.*"`
se preferir). Alternativas: no macOS, o do Xcode 27 já é o 21 (`xcrun clang-format --version`);
no Linux, o pacote `clang-format-21` da distribuição ou de apt.llvm.org; no Windows, o
instalador do LLVM 21. Formate só o que você mudou, nunca o repositório inteiro:

```sh
cmake --build --preset debug --target format-check
```

```sh
cmake --build --preset debug --target format
```

`format-check` falha se alguma linha alterada (staged, não staged ou arquivo novo) em relação
ao `HEAD` estiver fora do estilo; `format` reescreve só essas linhas. Para comparar com outra
revisão (ex.: numa branch), configure com `-DPOINTCLICK_FORMAT_BASE=origin/main`. Os alvos
usam apenas CMake, git e clang-format (sem shell nem Python), avisam quando o major instalado
difere do fixado e não fazem parte da build nem do `ctest`. Se o CMake achar outro executável,
aponte o certo com `-DPOINTCLICK_CLANG_FORMAT=<caminho>`. O [.editorconfig](.editorconfig)
dá ao editor a mesma base (UTF-8, LF, 4 espaços, 100 colunas).
