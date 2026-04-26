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

Escopo proposto:
- contar primeiro uso de textura;
- contar miss apos `SDLGameRenderer_UnlockPalette`;
- contar miss apos `SDLGameRenderer_UnlockTexture`;
- contar quantas texturas cacheadas foram invalidadas por paleta;
- contar quantas texturas cacheadas foram invalidadas por textura.

Objetivo:
- provar se o problema dominante e paleta, textura ou primeiro uso.

## 4.3 - Mapa de uso por handle

Escopo proposto:
- registrar handles mais afetados;
- evitar log gigante por frame, usando agregados;
- identificar se o churn vem de poucos handles ou de muitos handles.

Objetivo:
- escolher um alvo pequeno para prototipo.

## 4.4 - Prototipo isolado de caminho indexado

Escopo proposto:
- manter dados indexados separados da paleta;
- ativar somente com `--debug-indexed-texture-path`;
- preservar caminho padrao intacto.

Objetivo:
- comparar imagem e custo sem risco para o renderer atual.

## 4.5 - Comparacao visual e regressao

Escopo proposto:
- testar menu, versus, treino, golpes, especiais, flashes, sombras, HUD e cenarios;
- comparar logs do caminho padrao contra o caminho indexado.

Objetivo:
- decidir se o caminho experimental pode virar padrao, se deve ficar restrito, ou se deve ser descartado.
