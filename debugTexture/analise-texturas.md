# Analise inicial de texturas

## Hipotese atual

O contador `texture_cache_misses` mede recriacao de `SDL_Texture`, nao leitura de disco.

O fluxo provavel e:

1. O jogo atualiza uma paleta.
2. A camada PS2 chama `flUnlockPalette`.
3. O renderer SDL chama `SDLGameRenderer_UnlockPalette`.
4. A paleta antiga e destruida e as texturas cacheadas com essa paleta sao invalidadas.
5. No proximo `SDLGameRenderer_SetTexture`, a textura SDL e recriada a partir da surface paletizada.

## Por que isso importa

O hardware original e o port PS2 trabalham com uma ideia de textura indexada mais paleta/CLUT. O renderer SDL atual transforma essa combinacao em uma textura RGBA final. Quando a paleta muda, a textura final fica obsoleta.

## Regra de seguranca

Nao substituir o caminho de textura padrao sem antes:
- separar as causas dos misses;
- mapear quais handles sofrem mais invalidacao;
- comparar visualmente o caminho experimental contra o caminho atual;
- manter o experimento atras de `--debug-indexed-texture-path`.

## Micro etapa 4.2

A decomposicao dos misses e ativada somente quando `--debug-indexed-texture-path` tambem estiver ligado.

Campos adicionados:
- `texture_cache_misses_first_use`
- `texture_cache_misses_after_palette_unlock`
- `texture_cache_misses_after_texture_unlock`
- `texture_cache_misses_after_release`
- `texture_cache_misses_unknown`
- `palette_unlocks`
- `texture_unlocks`
- `palette_cache_invalidated_textures`
- `texture_cache_invalidated_textures`
- `release_cache_invalidated_textures`

Essa etapa ainda nao altera o caminho visual. Ela apenas explica por que uma textura SDL precisou ser recriada.
