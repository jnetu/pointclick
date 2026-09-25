# Relatório de Arquitetura, Modularidade e Portabilidade

**Projeto:** Point & Click Adventure Engine  
**Versão Atual:** 0.1.0  
**Linguagem & Bibliotecas:** C++20, SDL3 (via CPM)  
**Data:** 25 de Setembro de 2026  

---

## 1. Visão Geral e Diagnóstico Atual

O projeto atual estabelece uma base limpa e bem estruturada para a movimentação em perspectiva e edição de cenários. A separação em bibliotecas estáticas distintas ([`pointclick_core`](file:///Users/joao/CLionProjects/pointclick/CMakeLists.txt#L8) e [`pointclick_graphics`](file:///Users/joao/CLionProjects/pointclick/CMakeLists.txt#L22)) viabiliza testes unitários rápidos e desacoplados de bibliotecas de janelas e renderização.

```
                          ┌───────────────────────────┐
                          │     Application (SDL3)    │
                          │   Janela / Loop / Eventos │
                          └─────────────┬─────────────┘
                                        │
                 ┌──────────────────────┴──────────────────────┐
                 ▼                                             ▼
  ┌─────────────────────────────┐               ┌─────────────────────────────┐
  │     pointclick_graphics     │               │       pointclick_core       │
  │  - Renderer (SDL)           │               │  - Game (Orquestrador)      │
  │  - SceneRenderer            │──────────────►│  - Room & SceneObject       │
  │  - DiagnosticsOverlay       │               │  - Player                   │
  │  - DebugOverlay             │               │  - Navigation (A* 32px)     │
  └─────────────────────────────┘               │  - RoomFiles & Parameters   │
                                                │  - DebugEditor              │
                                                └─────────────────────────────┘
```

### Pontos Fortes Identificados
1. **Loop com Fixed-Timestep e Interpolação:** A classe [`Application`](file:///Users/joao/CLionProjects/pointclick/src/core/Application.cpp#L77-L118) atualiza a simulação em passos fixos (20 ticks/segundo) com acumulação de tempo e renderiza a 60 fps com interpolação linear (`alpha`) entre as posições anterior e atual dos pés do jogador. Isso garante comportamento determinístico.
2. **Navegação A\* Robusta com Otimização de Linha de Visão:** O módulo [`Navigation`](file:///Users/joao/CLionProjects/pointclick/src/game/Navigation.cpp) rasteriza obstáculos com padding de colisão e aplica simplificação de rota com teste de visibilidade direta (`clearLine`), resultando em trajetórias naturais sem passar por dentro de obstáculos.
3. **Catálogo Reflexivo de Parâmetros:** A abstração em [`RoomParameters`](file:///Users/joao/CLionProjects/pointclick/src/content/RoomParameters.cpp) permite adicionar campos e variáveis de salas com validação, passo e limites sem a necessidade de recriar a interface do editor ou a rotina de serialização.
4. **Resolução Lógica Integrada:** O uso de `SDL_SetRenderLogicalPresentation(..., 1280, 720, SDL_LOGICAL_PRESENTATION_LETTERBOX)` em [`Renderer.cpp`](file:///Users/joao/CLionProjects/pointclick/src/graphics/Renderer.cpp#L15-L19) resolve automaticamente o escalonamento e letterboxing em monitores e janelas com diferentes proporções.

---

## 2. Modularidade e Extensibilidade (Novas Mecânicas e Features)

Embora a base geométrica e de navegação esteja pronta, o motor ainda está fortemente centrado apenas na mecânica de caminhada. Para evoluir para um jogo completo de Point & Click, os seguintes pilares arquiteturais precisam ser introduzidos:

### 2.1. Sistema de Interação, Hotspots e Verbos
* **Situação Atual:** No método [`Game::onClick`](file:///Users/joao/CLionProjects/pointclick/src/game/Game.cpp#L71-L79), qualquer clique é tratado exclusivamente como ordem de andar. Se a coordenada não estiver dentro do chão (`!room_.containsFloor(position)`), o clique é ignorado.
* **Limitação:** Não é possível clicar em portas na parede, janelas, quadros ou itens no chão que estejam marcados como obstáculos sólidos.
* **Solução:**
  * Estender [`SceneObject`](file:///Users/joao/CLionProjects/pointclick/src/game/SceneObject.hpp) ou criar uma estrutura `InteractableHotspot`:
    * `name`: Nome legível exibido no cursor (ex: *"Porta da Biblioteca"*).
    * `interactionPoint`: Ponto no chão onde o jogador deve se posicionar para realizar a ação.
    * `interactionDirection`: Para onde o jogador deve olhar ao interagir.
    * `cursorType`: Ícone do ponteiro (examinar, pegar, conversar, usar).
  * No clique, realizar um *hit test*:
    1. Se clicou em um hotspot interativo: o jogador caminha até o `interactionPoint` e, ao alcançar o destino, dispara o evento de interação.
    2. Se clicou em chão livre: executa a movimentação comum.

### 2.2. Máquina de Estados do Jogador (`PlayerStateMachine`)
* **Situação Atual:** A classe [`Player`](file:///Users/joao/CLionProjects/pointclick/src/game/Player.hpp) monitora apenas se há waypoints restantes (`moving()`).
* **Limitação:** Não há suporte para animações de spritesheets, direção da face (Norte, Sul, Leste, Oeste) ou bloqueio de entrada durante animações/diálogos.
* **Solução:**
  * Implementar estados explícitos:
    * `PlayerState::Idle`
    * `PlayerState::Walking`
    * `PlayerState::Interacting`
    * `PlayerState::Talking`
    * `PlayerState::Locked` (durante cutscenes e trocas de sala).
  * Calcular a direção da face a partir do vetor de movimento $(\Delta x, \Delta y)$ para selecionar os frames corretos da animação.

### 2.3. Estado Global do Jogo (*Game State*) vs. Definição da Sala (*Room Template*)
* **Situação Atual:** O arquivo `.room` ([`RoomFiles::save`](file:///Users/joao/CLionProjects/pointclick/src/content/RoomFiles.cpp#L129)) serializa a geometria do cenário e os parâmetros visuais. Se o jogador sair da sala e voltar, a sala é recarregada do zero.
* **Limitação:** Inexistência de persistência para o progresso do jogo (inventário, portas destrancadas, quebra-cabeças resolvidos).
* **Solução:**
  * Separar os dados em duas camadas:
    1. **Dados Estáticos (Design):** Parâmetros imutáveis da sala carregados de `assets/rooms/*.room`.
    2. **Dados Dinâmicos da Partida (`GameState` / `SaveGame`):**
       * `Inventory`: Lista de IDs de itens obtidos (`chave_prateada`, `isqueiro`).
       * `WorldFlags`: Mapa de estados (`"porta_biblioteca_aberta" -> true`).
       * `RoomOverrides`: Estados alterados de objetos por sala (ex: objeto `chave_mesa` visibilidade = false).

### 2.4. Camada de UI e Roteamento de Entrada
* **Situação Atual:** O método [`Application::processEvents`](file:///Users/joao/CLionProjects/pointclick/src/core/Application.cpp#L120-L155) repassa o clique diretamente para o jogo, com exceção de uma checagem hardcoded para a caixa do painel de debug.
* **Solução:**
  * Implementar uma hierarquia de captura de eventos:
    $$\text{Evento de Entrada} \longrightarrow \text{UI Layer (Inventário / Diálogos / Menus)} \longrightarrow \text{Gameplay (Hotspots / Chão)}$$
  * Se a UI consumir o evento de toque/clique, a ordem de caminhada no jogo não é disparada.

### 2.5. Pipeline de Recursos e Áudio
* **Renderização:** [`SceneRenderer.cpp`](file:///Users/joao/CLionProjects/pointclick/src/graphics/SceneRenderer.cpp) atualmente renderiza formas geométricas com `SDL_RenderFillRect`. Para suportar spritesheets e fundos desenhados, será necessário um `ResourceManager` para gerenciar o ciclo de vida de `SDL_Texture`.
* **Áudio:** O SDL foi inicializado com `SDL_INIT_AUDIO` em [`Application::initialize`](file:///Users/joao/CLionProjects/pointclick/src/core/Application.cpp#L35), mas não há módulo de áudio implementado. É recomendável adicionar um `AudioManager` para gerenciar trilha sonora (BGM), passos e efeitos de interface (SFX).

---

## 3. Análise de Portabilidade (Outros Sistemas e Celulares)

O uso de C++20 padrão e SDL3 garante compatibilidade nativa com Desktop (macOS, Windows e Linux). Contudo, **para plataformas móveis (Android e iOS) ou Web (WebAssembly/Emscripten), existem barreiras arquiteturais que precisam de ajustes**:

| Área | Situação Atual | Impacto em Celulares (Android / iOS) | Solução Obrigatória |
| :--- | :--- | :--- | :--- |
| **Leitura de Assets** | `std::ifstream` em [`RoomFiles::load`](file:///Users/joao/CLionProjects/pointclick/src/content/RoomFiles.cpp#L67). | **Crítico (Crash/Falha):** No Android, os assets ficam empacotados dentro do `.apk` (arquivo zip). `std::ifstream` não tem acesso aos arquivos internos do pacote e falhará ao carregar `first.room`. | Usar a abstração de I/O do SDL3: **`SDL_IOStream`** (`SDL_IOFromFile` ou `SDL_LoadFile`). O SDL3 lê transparentemente de APKs no Android e bundles no iOS. |
| **Escrita / Saves** | Salva na pasta de assets (`assets/rooms`). | **Crítico:** Em dispositivos móveis, a pasta da aplicação é estritamente somente leitura. Gravar nela gera erro de permissão. | Gravar saves e dados do jogador no diretório de preferências retornado por **`SDL_GetPrefPath`**. |
| **Loop Principal** | Loop `while (running_)` com `std::this_thread::sleep_until` em [`Application.cpp`](file:///Users/joao/CLionProjects/pointclick/src/core/Application.cpp#L88). | **Crítico:** Em sistemas móveis (iOS/Android) e WebAssembly, loops bloqueantes na thread principal são proibidos (o watchdog do iOS encerra a aplicação; no Android impede o ciclo de vida da Activity). | Refatorar para a arquitetura padrão de callbacks do SDL3: **`SDL_AppInit`**, **`SDL_AppIterate`**, **`SDL_AppEvent`** e **`SDL_AppQuit`**. |
| **Teclado Virtual** | Chama [`SDL_StartTextInput`](file:///Users/joao/CLionProjects/pointclick/src/core/Application.cpp#L68) incondicionalmente na inicialização. | **Severo (Problema de UX):** No Android e iOS, isso faz o teclado virtual cobrir metade da tela assim que o jogo abre. | Invocar `SDL_StartTextInput` apenas quando o usuário focar em um campo de texto real. |
| **Eventos de Toque** | Trata apenas `SDL_EVENT_MOUSE_MOTION` e `SDL_EVENT_MOUSE_BUTTON_DOWN`. | **Médio:** Celulares não possuem o conceito de passar o mouse por cima (*hover*). Gestos de toque exigem tratamento de `SDL_EVENT_FINGER_DOWN`. | Adaptar a interação para toques diretos e criar feedback visual sob o dedo ou toque longo para inspecionar. |
| **Área Segura (*Safe Area*)** | Painéis posicionados com margem fixa de 12px ([`DiagnosticsOverlay`](file:///Users/joao/CLionProjects/pointclick/src/graphics/DiagnosticsOverlay.cpp#L33)). | **Médio:** Elementos de UI rentes à borda podem ficar cortados por câmeras frontais (notches/punch-holes) ou barras de navegação gestual. | Consultar **`SDL_GetWindowSafeArea`** para calcular o recuo seguro da interface em telas modernas. |
| **Modo Debug** | Ativado digitando `D-E-B-U-G` no teclado físico. | **Baixo (Dev):** Sem teclado físico no celular, o editor não pode ser aberto durante testes em dispositivo real. | Adicionar um atalho tátil para builds de desenvolvimento (ex: toque simultâneo com 3 dedos). |

---

## 4. Arquitetura do Loop SDL3 (Migração para Callbacks)

Para unificar o código entre Desktop (Windows/macOS/Linux), Mobile (Android/iOS) e WebAssembly sem código condicional (`#ifdef`), o padrão recomendado pelo SDL3 substitui o `main()` tradicional por 4 callbacks:

```cpp
// Exemplo de integração com a API de callbacks do SDL3
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

struct AppState {
    Application app;
};

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
    auto* state = new AppState();
    *appstate = state;
    if (!state->app.initialize()) {
        return SDL_APP_FAILURE;
    }
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate) {
    auto* state = static_cast<AppState*>(appstate);
    state->app.updateAndRender();
    return state->app.isRunning() ? SDL_APP_CONTINUE : SDL_APP_SUCCESS;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
    auto* state = static_cast<AppState*>(appstate);
    state->app.handleEvent(*event);
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
    if (appstate) {
        auto* state = static_cast<AppState*>(appstate);
        state->app.shutdown();
        delete state;
    }
}
```

---

## 5. Roteiro Prático Recomendado (Roadmap)

### Fase 1: Blindagem de Portabilidade (Técnica)
1. **Abstração de Arquivos:** Trocar `std::ifstream` por `SDL_LoadFile` ou `SDL_IOFromFile` em [`RoomFiles.cpp`](file:///Users/joao/CLionProjects/pointclick/src/content/RoomFiles.cpp).
2. **Separação de Pastas:** Usar `SDL_GetBasePath()` para leitura de assets e `SDL_GetPrefPath()` para salvamento de arquivos/progresso.
3. **Desativação de Teclado Automático:** Remover `SDL_StartTextInput` da inicialização da janela.
4. **Modernização do Loop:** Migrar [`Application`](file:///Users/joao/CLionProjects/pointclick/src/core/Application.cpp) para os callbacks nativos do SDL3.

### Fase 2: Expansão de Gameplay (Game Design)
1. **Entidades Interativas (*Hotspots*):** Adicionar ponto de interação e ação contextual em [`SceneObject`](file:///Users/joao/CLionProjects/pointclick/src/game/SceneObject.hpp).
2. **Máquina de Estados de Ação:** Fazer o personagem caminhar até o objeto antes de executar o evento de interação.
3. **Módulo de Estado do Jogo (`GameState`):** Implementar o gerenciamento de inventário e flags de progresso.
4. **Camada de UI:** Implementar uma barra de inventário na tela com prioridade sobre os cliques no cenário.

### Fase 3: Apresentação Audiovisual
1. **Módulo de Sprites:** Substituir o desenho de retângulos em [`SceneRenderer`](file:///Users/joao/CLionProjects/pointclick/src/graphics/SceneRenderer.cpp) por renderização de texturas e animações baseadas em tempo.
2. **Módulo de Áudio:** Implementar reprodução de música ambiente e efeitos sonoros com a API de áudio do SDL3 (`SDL_AudioStream`).
3. **Ajuste de Margens Seguras:** Integrar `SDL_GetWindowSafeArea` nas camadas de overlay e UI.
