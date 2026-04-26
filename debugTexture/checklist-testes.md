# Checklist de testes

## Micro etapa 4.1

1. Rodar sem flags.
   - Esperado: jogo abre como antes.
   - Esperado: se a pasta `data/debug` for removida, ela nao deve ser recriada.

2. Rodar com `--debug-mode`.
   - Esperado: `session.txt` contem `debug_indexed_texture_path=0`.
   - Esperado: `summary.txt`, `frame_timing.csv`, `render_stats.csv` e `event_log.csv` continuam sendo gerados.

3. Rodar com `--debug-mode --debug-indexed-texture-path`.
   - Esperado: `session.txt` contem `debug_indexed_texture_path=1`.
   - Esperado: nenhuma mudanca visual nesta micro etapa.

## Cenas importantes para proximas etapas

- menu inicial;
- selecao de personagem;
- treino com golpes normais;
- treino com especiais;
- treino com super arts;
- hit sparks;
- block sparks;
- sombras dos personagens;
- HUD, timer e barras;
- troca de round;
- retorno ao menu.

## Micro etapa 4.2

1. Rodar com `--debug-mode`.
   - Esperado: `render_stats.csv` contem as novas colunas.
   - Esperado: colunas detalhadas de textura ficam zeradas sem `--debug-indexed-texture-path`.

2. Rodar com `--debug-mode --debug-indexed-texture-path`.
   - Esperado: `render_stats.csv` contem a decomposicao dos misses.
   - Esperado: `summary.txt` contem os totais `total_texture_cache_misses_*`.
   - Esperado: `event_log.csv` contem eventos como `texture_cache_miss_first_use`, `palette_unlock` e `palette_cache_invalidated_textures` quando ocorrerem.

3. Conferir a soma.
   - Esperado: `total_texture_cache_misses` deve ser igual a soma de:
     - `total_texture_cache_misses_first_use`;
     - `total_texture_cache_misses_after_palette_unlock`;
     - `total_texture_cache_misses_after_texture_unlock`;
     - `total_texture_cache_misses_after_release`;
     - `total_texture_cache_misses_unknown`.

## Micro etapa 4.3

1. Rodar com `--debug-mode`.
   - Esperado: `texture_handle_stats.csv`, `palette_handle_stats.csv` e `texture_palette_handle_stats.csv` nao sao criados.

2. Rodar com `--debug-mode --debug-indexed-texture-path`.
   - Esperado: os tres CSVs por handle sao criados ao fechar o jogo.
   - Esperado: `texture_handle_stats.csv` mostra quais texturas concentram unlocks, misses e invalidacoes.
   - Esperado: `palette_handle_stats.csv` mostra quais paletas participam dos misses e invalidacoes.
   - Esperado: `texture_palette_handle_stats.csv` mostra as combinacoes textura/paleta que mais recriam `SDL_Texture`.

3. Teste de gameplay para esta etapa.
   - Entrar no treino.
   - Fazer golpes normais, especiais, super arts, blocks e hits.
   - Fechar pelo menu.
   - Ordenar os CSVs por `cache_misses`, `miss_after_texture_unlock` e `invalidated_by_texture_unlock`.

## Micro etapa 4.4

1. Rodar com `--debug-mode`.
   - Esperado: comportamento visual igual ao anterior.
   - Esperado: sem campos de update indexado relevantes no `summary.txt`.

2. Rodar com `--debug-mode --debug-indexed-texture-path`.
   - Esperado: jogo abre e renderiza corretamente.
   - Esperado: `total_indexed_texture_updates` maior que zero.
   - Esperado: `texture_cache_misses_after_palette_unlock` reduzido em relacao ao teste da 4.3.

3. Teste visual obrigatorio.
   - Verificar menu, selecao, round, HUD, sombras, hits, blocks, especiais e transicoes.
   - Se houver cor errada, sprite piscando, textura antiga ou frame visualmente incorreto, parar e analisar antes de ampliar o prototipo.

## Micro etapa 4.5

1. Rodar sem flags.
   - Esperado: nenhuma pasta/arquivo de debug e comportamento igual ao caminho normal.

2. Rodar com `--debug-mode`.
   - Esperado: linha base continua igual.
   - Esperado: campos do caminho indexado ficam zerados.

3. Rodar com `--debug-mode --debug-indexed-texture-path`.
   - Esperado: jogo abre e renderiza corretamente.
   - Esperado: `total_indexed_palette_updates` aparece no `summary.txt`.
   - Esperado: `total_indexed_texture_rgba_fallbacks=0` se o SDL/driver aceitou textura paletizada.
   - Esperado: `total_indexed_texture_update_pixels` representa uploads de indices, nao conversao RGBA massiva.
   - Se houver fallback: ordenar `texture_handle_stats.csv`, `palette_handle_stats.csv` e `texture_palette_handle_stats.csv` por `rgba_fallbacks`.

4. Teste de gameplay real.
   - Jogar ao menos um round completo no Arcade.
   - Fazer blocks, hits, especiais, super arts e transicao de round.
   - Fechar pelo menu.
   - Comparar contra uma sessao `--debug-mode` feita com o mesmo roteiro.

5. Bloqueios para encerrar a etapa.
   - Qualquer corrupcao visual em personagem, HUD, sombra, efeito ou tela de transicao.
   - `total_indexed_texture_rgba_fallbacks` maior que zero sem entender quais texturas cairam no fallback.
   - `worst_frame_ms`, `p95_frame_ms` ou `p99_frame_ms` piores que a linha base de forma relevante.
   - Sensacao de input pior, mesmo que os numeros parecam bons.
