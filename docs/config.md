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
- `integer`: Produces a pixel-perfect image, but requires a 4K display (⚠️ WARNING: the image will be cropped if your display resolution is smaller than 2688x2016)
- `square-pixels`: The internal buffer is scaled up by an integer (whole number) factor. Use this if you play on a CRT

### `bezel`

Displays the optional `data/img/bezel.png` image only in fullscreen mode on a 16:9 renderer output. Default value: `false`.

### `scanlines`

Applies scanlines only to the 4:3 game image in windowed and fullscreen modes. Default value: `false`.

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
- `integer`: Produz uma imagem pixel-perfect, mas requer um monitor 4K (⚠️ AVISO: a imagem será cortada se a resolução do monitor for menor que 2688x2016)
- `square-pixels`: O buffer interno é escalado por um fator inteiro. Use isso se jogar em um CRT

### `bezel`

Exibe a imagem opcional `data/img/bezel.png` somente no modo tela cheia quando a saída do renderer está em 16:9. Valor padrão: `false`.

### `scanlines`

Aplica scanlines somente à imagem 4:3 do jogo, tanto no modo janela quanto em tela cheia. Valor padrão: `false`.

### `scanline-opacity`

Controla a intensidade das scanlines entre `0` e `100`. Valor padrão: `20`.

### `draw-players-above-hud`

Permite que os personagens sejam renderizados na frente do HUD superior, similar ao Street Fighter IV. Pode introduzir anomalias visuais em alguns cenários.
