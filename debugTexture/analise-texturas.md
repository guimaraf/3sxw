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

## Micro etapa 4.3

A coleta por handle tambem e ativada somente com `--debug-indexed-texture-path`.

Arquivos:
- `texture_handle_stats.csv`: agregados por texture handle.
- `palette_handle_stats.csv`: agregados por palette handle.
- `texture_palette_handle_stats.csv`: agregados por combinacao texture handle + palette handle.

Campos principais:
- `set_texture_calls`: quantas vezes a combinacao foi solicitada para render;
- `cache_hits`: quantas vezes a textura SDL ja estava pronta;
- `cache_misses`: quantas vezes a textura SDL precisou ser criada;
- `miss_after_texture_unlock`: recriacao apos `SDLGameRenderer_UnlockTexture`;
- `miss_after_palette_unlock`: recriacao apos `SDLGameRenderer_UnlockPalette`;
- `invalidated_by_texture_unlock`: texturas SDL descartadas por unlock de textura;
- `invalidated_by_palette_unlock`: texturas SDL descartadas por unlock de paleta.

Interpretacao:
- poucos handles com muitos misses indicam alvo bom para prototipo;
- muitos handles com poucos misses indicam churn distribuido, mais arriscado de otimizar pontualmente;
- muitos `miss_after_texture_unlock` reforcam que o problema vem de atualizacao de dados de textura, nao de troca de paleta.

## Micro etapa 4.4

O prototipo nao usa shader. O SDL renderer atual nao expoe um caminho simples para aplicar paleta separada em GPU. Por isso, a primeira versao experimental mantem a API de render atual e troca apenas a estrategia interna das texturas paletizadas:

- cria `SDL_Texture` streaming em formato `SDL_PIXELFORMAT_RGBA32`;
- converte os pixels indexados usando a paleta atual;
- em `UnlockPalette`, marca as texturas streaming associadas como dirty;
- em `UnlockTexture`, marca as variantes streaming da textura como dirty;
- em `SetTexture`, atualiza de forma lazy apenas a combinacao textura/paleta realmente solicitada;
- se a textura ja foi usada no frame atual, o caminho preserva a semantica antiga e deixa a recriacao acontecer.

Essa abordagem ainda nao e o modelo final de textura indexada + paleta separada, mas testa a parte mais importante com baixo risco: evitar destruir/recriar `SDL_Texture` a cada atualizacao de paleta.

Novos campos:
- `indexed_texture_updates`
- `indexed_texture_update_pixels`
- `indexed_texture_update_ms`
- `total_indexed_texture_updates`
- `total_indexed_texture_update_pixels`
- `total_indexed_texture_update_ms`
- `worst_indexed_texture_update_ms`
