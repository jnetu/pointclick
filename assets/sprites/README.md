# Sprites

Coloque aqui os PNGs referenciados pelas salas (`assets/rooms/*.room`) e pelos atores
(`assets/actors/*.actor`). Os caminhos nos clips são relativos a esta pasta; subpastas
funcionam (por exemplo, `cenario/porta.png`).

O player compartilhado (`assets/actors/player.actor`) usa `player_idle.png`,
`player_esquerda.png`, `player_direita.png`, `player_cima.png` e `player_baixo.png`.

Quadros, margens, velocidade e tamanho visual de cada clip estão descritos em
[Sprites e animações](../../docs/GAME_DESIGN.md#sprites-e-animações) no guia de game design.
Um PNG ausente ou com corte inválido aparece no jogo como um quadriculado roxo e preto. Depois
de trocar um PNG com o jogo aberto, use `reload` no modo DEBUG.
