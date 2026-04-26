# Micro etapas da Fase 4

## 4.1 - Estrutura e flag experimental

Escopo:
- criar a pasta `debugTexture/`;
- adicionar `--debug-indexed-texture-path`;
- registrar a flag em `session.txt` quando `--debug-mode` estiver ativo;
- nao alterar o caminho visual do renderer.

Teste esperado:
- sem flags: nenhuma pasta debug nova e nenhum comportamento diferente;
- com `--debug-mode`: logs iguais aos anteriores, mais `debug_indexed_texture_path=0`;
- com `--debug-mode --debug-indexed-texture-path`: logs com `debug_indexed_texture_path=1`.

## 4.2 - Separar causas dos misses

Escopo:
- contar primeiro uso de textura;
- contar miss apos `SDLGameRenderer_UnlockPalette`;
- contar miss apos `SDLGameRenderer_UnlockTexture`;
- contar miss apos release de textura/paleta;
- contar miss de causa desconhecida;
- contar quantas texturas cacheadas foram invalidadas por paleta;
- contar quantas texturas cacheadas foram invalidadas por textura.

Objetivo:
- provar se o problema dominante e paleta, textura ou primeiro uso.

Teste esperado:
- com `--debug-mode`: colunas novas aparecem zeradas, mantendo o comportamento anterior;
- com `--debug-mode --debug-indexed-texture-path`: colunas novas passam a separar a causa dos misses;
- `texture_cache_misses` deve continuar representando o total antigo.

## 4.3 - Mapa de uso por handle

Escopo:
- registrar handles mais afetados em CSVs agregados;
- evitar log gigante por frame;
- identificar se o churn vem de poucas texturas, poucas paletas ou muitas combinacoes textura/paleta.

Objetivo:
- escolher um alvo pequeno para prototipo.

Arquivos gerados com `--debug-mode --debug-indexed-texture-path`:
- `texture_handle_stats.csv`
- `palette_handle_stats.csv`
- `texture_palette_handle_stats.csv`

Teste esperado:
- com `--debug-mode`: esses CSVs nao devem ser criados;
- com `--debug-mode --debug-indexed-texture-path`: os CSVs devem ser criados ao fechar o jogo;
- os maiores valores de `cache_misses`, `miss_after_texture_unlock` e `invalidated_by_texture_unlock` indicam os handles candidatos para o prototipo.

## 4.4 - Prototipo isolado de caminho indexado

Escopo:
- criar um caminho experimental para texturas paletizadas;
- trocar `SDL_CreateTextureFromSurface` por textura streaming RGBA atualizavel no modo experimental;
- marcar texturas cacheadas como dirty em `UnlockPalette` e `UnlockTexture`;
- atualizar de forma lazy somente a combinacao textura/paleta solicitada por `SetTexture`;
- ativar somente com `--debug-indexed-texture-path`;
- preservar caminho padrao intacto.

Objetivo:
- reduzir recriacao/destruicao de `SDL_Texture` sem alterar o modo normal.
- comparar imagem e custo contra o caminho atual.

Teste esperado:
- com `--debug-mode`: comportamento e logs do caminho padrao continuam iguais;
- com `--debug-mode --debug-indexed-texture-path`: `texture_cache_misses_after_palette_unlock` deve cair bastante se o prototipo estiver funcionando;
- `summary.txt` passa a registrar `total_indexed_texture_updates`, `total_indexed_texture_update_pixels` e tempo de update;
- qualquer regressao visual deve ser tratada como bloqueio antes de avançar.

## 4.5 - Comparacao visual e regressao

Escopo proposto:
- testar menu, versus, treino, golpes, especiais, flashes, sombras, HUD e cenarios;
- comparar logs do caminho padrao contra o caminho indexado.

Objetivo:
- decidir se o caminho experimental pode virar padrao, se deve ficar restrito, ou se deve ser descartado.
