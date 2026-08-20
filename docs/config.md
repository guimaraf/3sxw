# Config

3SXW supports a config file which allows you to change several useful options.

Config location (all platforms):
- `<game folder>/data/config`

> **Difference from the original project:** The upstream stored the config in OS-specific directories (e.g. `AppData\Roaming\CrowdedStreet\3SX\config` on Windows). In this fork, the config file is always kept next to the executable, making the game portable.

## Options

### `fullscreen`

Whether the game should start in fullscreen mode.

### `window-width` / `window-height`

Window dimensions to use when `fullscreen` is set to `false`.

### `scale-mode`

The way the internal 384x224 buffer is scaled.

Default value: `nearest`.

Possible values:
- `nearest`
- `linear`
- `soft-linear`: Produces an image with a balance of sharpness and sizing consistency
- `square-pixels`: Uses a corrected 300x224 presentation grid. At 1920x1080 it produces a 1200x896 game area aligned with the Capcom bezel

### `aspect-ratio`

Controls the presentation shape. Default value: `preserve`.

Possible values:
- `preserve`: Keeps the original 4:3 image with black bars when needed
- `stretch`: Fills the complete output horizontally and vertically. The configurator changes `square-pixels` to `nearest` and clears the bezel setting in this mode

### `frame-timing`

Controls the full game frame cadence. Default value: `arcade`.

Possible values:
- `arcade`: 59.59949 FPS, preserving the port's previous timing
- `ps2`: 60000/1001 FPS (approximately 59.94)

### `bezel`

Displays an optional bezel only in fullscreen mode on a 16:9 renderer output while `aspect-ratio` is `preserve`. With `scale-mode = square-pixels`, the game loads `data/img/bezel-pixel-perfect.png`; all other modes load `data/img/bezel.png`. Default value: `false`.

### `scanlines`

Applies scanlines only to the rendered game image in windowed and fullscreen modes, including preserved 4:3 and stretched presentation. Default value: `false`.

### `scanline-opacity`

Controls scanline intensity from `0` to `100`. Default value: `20`.

### `draw-players-above-hud`

Allow characters to render in front of the top HUD similar to Street Fighter IV. May introduce visual abnormalities on certain stages.

---
---

<!--  🇧🇷 ──────────────────── PORTUGUÊS ────────────────────── 🇧🇷  -->

---

# Config

O 3SXW suporta um arquivo de configuração que permite alterar diversas opções úteis.

Localização do arquivo de configuração (todas as plataformas):
- `<pasta do jogo>/data/config`

> **Diferença em relação ao projeto original:** o upstream armazenava a configuração em diretórios específicos do sistema operacional (ex.: `AppData\Roaming\CrowdedStreet\3SX\config` no Windows). Neste fork, o arquivo de configuração fica sempre ao lado do executável, tornando o jogo portátil.

## Opções

### `fullscreen`

Define se o jogo deve iniciar em modo tela cheia.

### `window-width` / `window-height`

Dimensões da janela a serem usadas quando `fullscreen` está definido como `false`.

### `scale-mode`

A forma como o buffer interno de 384x224 é escalado.

Valor padrão: `nearest`.

Valores possíveis:
- `nearest`
- `linear`
- `soft-linear`: Produz uma imagem com equilíbrio entre nitidez e consistência de tamanho
- `square-pixels`: Usa uma grade de apresentacao corrigida de 300x224. Em 1920x1080 produz uma area de jogo de 1200x896 alinhada com a moldura Capcom

### `aspect-ratio`

Controla o formato de apresentacao. Valor padrao: `preserve`.

Valores possiveis:
- `preserve`: Mantem a imagem original em 4:3 com barras pretas quando necessario
- `stretch`: Preenche toda a saida horizontal e verticalmente. O configurador altera `square-pixels` para `nearest` e desmarca a moldura neste modo

### `frame-timing`

Controla a cadencia completa dos frames do jogo. Valor padrao: `arcade`.

Valores possiveis:
- `arcade`: 59.59949 FPS, preservando a cadencia anterior do port
- `ps2`: 60000/1001 FPS (aproximadamente 59.94)

### `bezel`

Exibe uma moldura opcional somente no modo tela cheia quando a saída do renderer está em 16:9 e `aspect-ratio` usa `preserve`. Com `scale-mode = square-pixels`, o jogo carrega `data/img/bezel-pixel-perfect.png`; os demais modos carregam `data/img/bezel.png`. Valor padrão: `false`.

### `scanlines`

Aplica scanlines somente à imagem renderizada do jogo, tanto no modo janela quanto em tela cheia, incluindo apresentação 4:3 preservada e esticada. Valor padrão: `false`.

### `scanline-opacity`

Controla a intensidade das scanlines entre `0` e `100`. Valor padrão: `20`.

### `draw-players-above-hud`

Permite que os personagens sejam renderizados na frente do HUD superior, similar ao Street Fighter IV. Pode introduzir anomalias visuais em alguns cenários.
