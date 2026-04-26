# debugTexture

Esta pasta registra a analise e os passos experimentais do caminho de textura indexada.

Objetivo:
- investigar se o renderer SDL esta recriando texturas por mudancas de paleta;
- separar misses de cache por causa antes de otimizar;
- prototipar um caminho experimental ativado apenas com `--debug-indexed-texture-path`;
- evitar mudancas amplas no render padrao sem comparacao visual e numerica.

Estado atual:
- micro etapa 4.1 criou a flag experimental e a documentacao;
- micro etapa 4.2 separa as causas dos misses quando a flag experimental esta ligada;
- micro etapa 4.3 gera CSVs agregados por texture handle, palette handle e combinacao texture/palette;
- a flag ainda nao muda o algoritmo de renderizacao;
- o caminho padrao continua usando textura SDL RGBA criada a partir da surface paletizada.

Como testar esta micro etapa:
- rodar normalmente, sem flags: comportamento esperado igual ao estado anterior;
- rodar com `--debug-mode`: logs atuais devem continuar iguais, com uma linha adicional `debug_indexed_texture_path=0`;
- rodar com `--debug-mode --debug-indexed-texture-path`: logs devem mostrar `debug_indexed_texture_path=1`, ainda sem mudanca visual esperada.
