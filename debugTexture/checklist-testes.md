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
