# GD AUTOPILOT

**Advanced AI Geometry Dash Level Completion Bot — Geode SDK Mod (C++23)**

GD AUTOPILOT é um mod modular nativo em C++ para **Geometry Dash 2.2081** desenvolvido com o **Geode SDK v5.11.2**, projetado para analisar fases em tempo real, simular física a 240Hz, adaptar decisões com aprendizado de tentativas anteriores e jogar fases automaticamente em todos os 8 modos do jogo.

---

## Arquitetura Modular

```text
gd-autopilot/
├── .github/workflows/build.yml        # Pipeline CI/CD (Testes C++23 + Build multi-plataforma Geode + .geode)
├── CMakeLists.txt                     # Configuração oficial Geode SDK (C++23)
├── mod.json                           # Metadados e configurações nativas do Geode v5.11.2
├── about.md                           # Descrição exibida no Geode Index
├── src/
│   ├── main.cpp                       # Hooks nativos Geode (GJBaseGameLayer, PlayLayer, PauseLayer, MenuLayer)
│   ├── core/
│   │   ├── Types.hpp                  # Estruturas de estado, física, padrões, decisões e estatísticas
│   │   └── DebugLog.hpp / .cpp        # Logger ring-buffer integrado ao Debug Console e geode::log
│   └── modules/
│       ├── AIPlayer.hpp / .cpp        # 1. Orquestrador principal integrado ao PlayLayer / PlayerObject
│       ├── AIEngine.hpp / .cpp        # 2. Motor de decisão em tempo real com rollout multi-branch
│       ├── GameController.hpp / .cpp  # 3. Controladores dedicados (Cube, Ship, Ball, UFO, Wave, Robot, Spider, Swing)
│       ├── PhysicsEngine.hpp / .cpp   # 4. Motor de simulação física 240Hz (gravidade, velocidade, portais, pads, orbs, slopes)
│       ├── LevelAnalyzer.hpp / .cpp   # 5. Extração de objetos, índice espacial O(log N), padrões e Safe Corridor Map
│       ├── GeodeInterface.hpp / .cpp  # 6. Interface Geode v5 (Popup 6 abas + HUD Overlay + botões no Pause/Menu)
│       ├── LearningSystem.hpp / .cpp  # 7. Memória persistente por fase, zonas de falha e rotação adaptativa de estratégias
│       └── StatisticsManager.hpp/.cpp # 8. Telemetria de tentativas, progresso, tempo de execução e latência
└── tests/
    └── test_autopilot.cpp             # Suíte de testes unitários C++23 standalone
```

---

## Módulos Implementados

1. **AIPlayer (`src/modules/AIPlayer.*`)**: Lê em tempo real o estado do `PlayerObject`, gerencia ciclo de vida do `PlayLayer` (`init`, `setupHasCompleted`, `resetLevel`, `destroyPlayer`, `levelComplete`, `onQuit`) e despacha comandos via `GJBaseGameLayer::handleButton`.
2. **AIEngine (`src/modules/AIEngine.*`)**: Avalia múltiplos planos de ação em paralelo sobre o `PhysicsEngine`, combinando pontuação de sobrevivência, distância mínima de hazards, alinhamento ao corredor seguro, heurísticas do modo e vieses aprendidos.
3. **GameController (`src/modules/GameController.*`)**: 8 controladores polimórficos independentes (`CubeController`, `ShipController`, `BallController`, `UFOController`, `WaveController`, `RobotController`, `SpiderController`, `SwingController`) com controle de borda de clique/soltura para modos baseados em tap e orbs.
4. **PhysicsEngine (`src/modules/PhysicsEngine.*`)**: Simula gravidade normal/invertida, velocidades `0.5x` a `4x`, modo mini/normal, colisões AABB + slopes, 9 tipos de orbs, 5 tipos de pads e portais de modo/gravidade/velocidade/tamanho.
5. **LevelAnalyzer (`src/modules/LevelAnalyzer.*`)**: Classifica objetos usando IDs canônicos do GD 2.2 + `GameObjectType`, constrói índice ordenado por X, gera o **Safe Corridor Map** suavizado e detecta padrões (`SingleSpike`, `SpikeCluster`, `OrbChain`, `PadLaunch`, `NarrowCorridor`, `GravityTransition`, `ModeTransition`).
6. **GeodeInterface (`src/modules/GeodeInterface.*`)**:
   - Popup nativo Geode v5 com 6 abas: **AI PLAYER**, **LEVEL ANALYZER**, **PHYSICS ENGINE**, **AI SETTINGS**, **STATISTICS**, **DEBUG CONSOLE**.
   - Controles fixos de **START**, **STOP** e **RE-ANALYZE**, indicador colorido de atividade, indicador do modo atual, barra de progresso ao vivo e preset de velocidade de análise (`1-ECO` até `4-ULTRA`).
   - **HUD Overlay** discreto durante a fase e botões de acesso rápido no `PauseLayer` e `MenuLayer`.
7. **LearningSystem (`src/modules/LearningSystem.*`)**: Persiste dados em `Mod::get()->getSaveDir()/learning/level_<ID>.gda`, agrupa mortes recorrentes em `FailureZone`, alterna estratégias adaptativas e promove sequências bem-sucedidas após avanço ou conclusão.
8. **StatisticsManager (`src/modules/StatisticsManager.*`)**: Contabiliza tentativas, progresso atual, melhor progresso, conclusões, tempo de sessão/tentativa, modo atual, estado da IA e latência média de decisão.

---

## Compilação Automática via GitHub Actions

O workflow `.github/workflows/build.yml` já vem pronto e executa automaticamente a cada `push`, `pull_request` ou disparo manual (`workflow_dispatch`):

1. **`unit-tests`**: Compila e executa `tests/test_autopilot.cpp` em C++23 (`g++ -std=c++23 -Wall -Wextra -Wpedantic`).
2. **`build`**: Compila o mod com a action oficial `geode-sdk/build-geode-mod@main` para **5 plataformas**:
   - `Win64` (Windows)
   - `MacOS` (macOS Apple Silicon + Intel)
   - `iOS` (arm64)
   - `Android32` (armeabi-v7a)
   - `Android64` (arm64-v8a)
3. **`package`**: Combina todos os binários em um único pacote `.geode` usando `geode-sdk/build-geode-mod/combine@main`, faz upload no artifact `gd-autopilot-geode` e cria automaticamente uma **GitHub Release** quando uma tag `v*` é enviada (ex.: `git tag v1.0.0 && git push origin v1.0.0`).

### Como baixar o `.geode` compilado pelo GitHub Actions

1. Crie um repositório no GitHub e envie a pasta `gd-autopilot` para o repositório.
2. Abra a aba **Actions** no GitHub e aguarde o workflow **Build GD AUTOPILOT** concluir.
3. Clique no run finalizado e baixe o artifact **`gd-autopilot-geode`** (contendo o arquivo `.geode`).

---

## Compilação Local

### Requisitos

- **Geometry Dash 2.2081** com **Geode Loader v5.11.2** instalado ([geode-sdk.org](https://geode-sdk.org/install)).
- **Geode CLI** e **Geode SDK v5.11.2** configurados (`GEODE_SDK` definido no ambiente).
- **CMake >= 3.21** e compilador com suporte a **C++23** (MSVC 2022+, Clang 16+ ou GCC 13+).

### Passos para compilar o mod `.geode`

```bash
# 1. Instalar/atualizar SDK do Geode (caso ainda não tenha)
geode sdk install
geode sdk install-binaries

# 2. Compilar o mod dentro da pasta gd-autopilot/
cd gd-autopilot
geode build
```

O arquivo `.geode` compilado será gerado em `build/gdautopilot.gd-autopilot.geode`.

### Executar os Testes Unitários Core (sem precisar do GD aberto)

```bash
g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -DGD_AUTOPILOT_STANDALONE -Isrc \
  src/core/DebugLog.cpp \
  src/modules/PhysicsEngine.cpp \
  src/modules/LevelAnalyzer.cpp \
  src/modules/GameController.cpp \
  src/modules/LearningSystem.cpp \
  src/modules/AIEngine.cpp \
  src/modules/StatisticsManager.cpp \
  tests/test_autopilot.cpp \
  -o build_test_autopilot

./build_test_autopilot
```

---

## Instalação e Uso no Geometry Dash

1. Copie `gdautopilot.gd-autopilot.geode` para a pasta `geode/mods/` do seu Geometry Dash (ou importe diretamente pelo menu do Geode no Android/PC).
2. Inicie o Geometry Dash.
3. No **Menu Principal**, clique no botão **`AI`** (menu inferior) ou pause qualquer fase e clique em **`GD AUTOPILOT`**.
4. Clique em **`START`** para ativar o bot automaticamente. Acompanhe o progresso e o modo atual pelo **HUD Overlay** ou pelas 6 abas do painel.

> **Nota sobre compatibilidade de versão:** Este projeto foi validado contra os bindings oficiais do **Geode SDK v5.11.2 / Geometry Dash 2.2081**. Caso compile para uma versão futura do Geode/GD, atualize os campos `"geode"` e `"gd"` em `mod.json`.
