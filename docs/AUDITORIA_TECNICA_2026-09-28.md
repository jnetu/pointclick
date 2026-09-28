# Auditoria técnica preliminar — Point & Click

**Data:** 28/09/2026 · **Escopo:** código, build, documentação, salas, sprites e testes existentes.

## Síntese

Para o protótipo atual, a base é coerente: `game/`, `content/` e `graphics/` têm papéis distintos; as regras não desenham diretamente; salas são validadas antes de aplicar; e o loop usa passo fixo com interpolação. A navegação e os cinco PNGs atuais são pequenos. Não encontrei motivo para introduzir agora ECS, uma engine própria ou uma refatoração ampla.

Isso ainda não demonstra compatibilidade com iOS/Android/consoles nem desempenho medido em hardware fraco. Os riscos mais importantes antes de expandir o escopo são:

## Pontos críticos

1. **Empacotamento, leitura e gravação multiplataforma — prioridade alta.** O CMake configura executáveis e cópia de assets para desktop; não há configuração de app, bundle ou pacote para iOS/Android/consoles. `RoomFiles::load/save` usa `std::ifstream/ofstream` e o editor grava junto aos arquivos de sala. No Android, os assets entram no APK e devem ser lidos pela camada de I/O do SDL; saves precisam ir para uma pasta gravável da aplicação, como `SDL_GetPrefPath`. Separe leitura de conteúdo empacotado de gravação de progresso/editor e crie os alvos de build por plataforma. A [documentação Android do SDL 3.4.16](https://github.com/libsdl-org/SDL/blob/release-3.4.16/docs/README-android.md#loading-assets) descreve o empacotamento e a leitura de assets; [SDL_GetPrefPath](https://wiki.libsdl.org/SDL3/SDL_GetPrefPath) é destinado a preferências e saves.
2. **Ciclo de vida e entrada em mobile — prioridade alta.** `Application` só trata `SDL_EVENT_QUIT`, inicia `SDL_StartTextInput` no startup e não trata eventos de suspensão, pouca memória ou restauração do dispositivo de renderização. No iOS, iniciar entrada de texto também revela o teclado virtual; faça isso apenas quando um campo de texto estiver em foco. O SDL pode converter toque em evento de mouse, então o toque básico pode funcionar; ainda faltam tratamento explícito de gestos, feedback sem hover, safe area e roteamento de entrada para jogo/UI. Veja as notas oficiais de [iOS](https://wiki.libsdl.org/SDL3/README-ios), [touch](https://wiki.libsdl.org/SDL3/CategoryTouch) e [ciclo de vida Android](https://github.com/libsdl-org/SDL/blob/release-3.4.16/docs/README-android.md#activity-lifecycle).
3. **Câmera e mundo maior — definir antes de criar salas extensas.** O mundo é fixo em 1280×720, a validação rejeita objetos fora da tela e a grade de navegação tem dimensão fixa. Uma câmera futura exigirá distinguir coordenadas do mundo das coordenadas da viewport, converter entrada nos dois sentidos e definir limites por sala. Mantenha UI independente da câmera e decida se mobile será paisagem fixa ou responsivo.
4. **Interações, diálogos e progresso — expansão planejada, ainda ausente.** Hoje `Game::onClick` só transforma cliques no chão em rotas; objetos são decoração ou obstáculo. Antes de diálogos/inventário, mantenha `Room` como definição autorada e crie estado de sessão/save separado para flags, itens e estado dos objetos. Um sistema de interação pode selecionar um alvo, caminhar até um ponto de interação e então emitir uma ação; diálogo mantém estado e conteúdo próprios. Encaminhe eventos por modo (jogo, diálogo, editor). Versione o formato `.room` antes que muitos arquivos dependam dele.
5. **Desempenho e memória — baixo risco no conteúdo atual; medir ao crescer.** Há cinco spritesheets pequenos e a navegação atual tem 920 células; a cena tem poucos objetos e não usa shaders ou partículas pesadas. O código, porém, aloca e ordena uma lista de desenho a cada quadro (`SceneRenderer`), valida e percorre os quadros do clip em cada desenho (`SpriteLibrary`), limpa todo o cache de texturas quando qualquer revisão da sala muda (`Renderer`) e pode tentar A* repetidamente ao clicar em área bloqueada (`Navigation`). São otimizações para medir conforme aumentarem objetos e salas; não indicam lentidão comprovada hoje. Registre o backend escolhido — `SDL_CreateRenderer(..., nullptr)` deixa o SDL escolher o melhor disponível — e faça um perfil em renderizador por software, baixa memória e telas de alta densidade antes de definir requisitos mínimos. [SDL_CreateRenderer](https://wiki.libsdl.org/SDL3/SDL_CreateRenderer).

## Ordem sugerida

1. Definir leitura de assets, pasta gravável e empacotamento para cada alvo real.
2. Adicionar tratamento de ciclo de vida e uma camada de entrada comum para mouse/toque/controle.
3. Projetar mundo/câmera/viewport antes de ampliar cenários.
4. Introduzir interação, diálogo e save em módulos separados, reaproveitando IDs estáveis e a validação existente.
5. Medir em dispositivos modestos e otimizar os caminhos de renderização/rota que aparecerem no perfil.

**Limite desta auditoria:** os testes existentes foram inspecionados, mas não executados; não fiz benchmark nem teste em dispositivos. O `relatorio.md` existente é de 25/09 e contém trechos já superados pelo `SpriteLibrary` presente no código; este documento registra a avaliação atual sem sobrescrevê-lo.
