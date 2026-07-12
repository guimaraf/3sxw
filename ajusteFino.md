# Ajustes finos do projeto 3SXW

Este documento reúne os pontos levantados durante a análise do projeto. O objetivo é registrar o diagnóstico técnico, as decisões já tomadas para este fork e as alternativas de implementação antes de modificar o código.

Nenhuma proposta descrita abaixo está automaticamente aprovada para implementação. Cada grupo pode ser analisado e decidido separadamente.

## Status da primeira implementação

Em 12 de julho de 2026, foi aplicada uma primeira rodada das etapas A a E, ainda pendente de build e validação local pelo mantenedor:

- propagação das falhas de inicialização SDL, renderer e AFS até o loop principal;
- mensagem visível e `data/error.log` para erros críticos, inclusive em Release;
- cleanup parcial e encerramento explícito de gamepads, renderer, ADX e SPU;
- teste de criação, escrita, leitura e remoção dentro de `data/` antes de iniciar o jogo;
- preservação do Memory Card virtual sempre presente e da criação sob demanda de `slot1/` e `slot2/`;
- retorno de erros físicos reais nas operações de arquivo do Memory Card;
- validação dos canvases, surfaces, palettes, handles e texturas dinâmicas;
- troca segura de `screen_texture` durante resize, mantendo a anterior se a nova criação falhar;
- manutenção integral das filas, caches e limites existentes do renderer;
- tolerância a dispositivo de áudio indisponível, sem alterar filas assíncronas ou transições por round;
- validação da cópia e abertura de `resources/SF33RD.AFS`, com remoção de arquivo parcial;
- caminho completo do recurso obrigatório nas mensagens ao usuário;
- remoção das consultas, ticks e overlays online do caminho de execução offline;
- desativação de `NETPLAY_ENABLED` também em Debug;
- uso do diretório ao lado de `3SX.app` como base portátil no macOS.

As seções seguintes permanecem como histórico técnico e justificativa das decisões.

## 1. Escopo deste fork

### Decisões confirmadas

- O fork será focado exclusivamente nos modos offline.
- Não haverá suporte a netplay, rollback ou outros recursos online.
- O código de rollback pode ser ignorado enquanto não afetar os modos offline.
- Configurações, saves, replays, logs e demais arquivos gerados devem permanecer na pasta do jogo.
- Não deve haver fallback para `AppData`, diretórios do usuário ou pastas específicas do sistema operacional.
- O objetivo é manter a distribuição portátil: copiar a pasta do jogo deve ser suficiente para transportar toda a instalação e os dados do usuário.
- Os testes realizados até agora foram feitos no Windows.
- O usuário fará os builds localmente; o Codex não deve executar builds.

### Documentação já corrigida

O `README.md` foi ajustado para:

- remover menções diretas e indiretas a netplay, rollback, P2P, GekkoNet e SDL_net;
- corrigir os diretórios documentados em português.

Diretórios reais confirmados no código:

| Tipo | Diretório ou arquivo |
|---|---|
| Saves e progresso do slot 1 | `data/saves/slot1/` |
| Saves e progresso do slot 2 | `data/saves/slot2/` |
| Replays | `data/saves/slot1/` e `data/saves/slot2/` |
| Configuração | `data/config` |
| Mapeamento de teclas | `data/keymap` |
| Diagnóstico em runtime | `data/debug/` |
| Erros fatais | `data/error.log` |

O arquivo `docs/config.md` já apontava para o caminho correto.

## 2. Ordem atual de inicialização

O fluxo atual pode ser resumido assim:

```text
SDLApp_PreInit()
└── inicialização de vídeo SDL

Verificação ou cópia dos recursos do jogo

SDLApp_FullInit()
├── Config_Init()
├── Keymap_Init()
├── inicialização de áudio e gamepad SDL
├── criação da janela e renderer
├── criação dos canvases e texturas principais
└── inicialização dos controles

AFS_Init()

sf3_init()
├── inicialização do sistema interno do jogo
├── Init_sound_system()
│   ├── ADX
│   └── SPU/efeitos sonoros
└── MemcardInit()
```

### Conclusão sobre SDL e Memory Card

O Memory Card não depende de ignorar erros da inicialização SDL. Ele é inicializado depois da janela, renderer, AFS, áudio e outros sistemas.

Se a inicialização de vídeo ou renderer falhar, o programa não terá como mostrar corretamente a interface do jogo nem as telas do Memory Card. Nesse caso, continuar a execução apenas desloca o erro para outro ponto e aumenta a possibilidade de crash por ponteiro nulo.

A inicialização do sistema de arquivos da SDL é diferente da inicialização explícita dos subsistemas de vídeo, áudio e gamepad. Operações como `SDL_IOFromFile()` estão disponíveis sem que áudio ou vídeo tenham sido inicializados. Portanto, validar `SDL_Init()` não impede o funcionamento do Memory Card.

### Problemas atuais confirmados

Em `src/main.c`:

- o retorno de `SDLApp_PreInit()` é ignorado;
- o retorno de `SDLApp_FullInit()` é ignorado;
- o retorno de `AFS_Init()` é ignorado;
- `initialize_game()` não informa ao loop se a inicialização falhou;
- `cleanup()` é chamado mesmo se alguns subsistemas não tiverem sido inicializados;
- fechar diretamente a janela pode não passar pelo mesmo encerramento de áudio usado pela opção de saída do menu.

### Classificação recomendada dos erros

| Subsistema | Tratamento recomendado | Motivo |
|---|---|---|
| Vídeo SDL | Fatal | O jogo não pode apresentar interface sem vídeo. |
| Janela | Fatal | Não há destino de apresentação. |
| Renderer | Fatal | Todo o frontend depende do renderer. |
| Canvas principal do CPS3 | Fatal | É o alvo central do desenho do jogo. |
| Canvas de mensagens | Fatal | Telas e mensagens do jogo dependem dele. |
| AFS/recursos principais | Fatal | O conteúdo necessário para executar o jogo não está disponível. |
| Pasta portátil sem escrita | Fatal | O requisito do fork é que save e configuração funcionem na pasta do jogo. |
| Gamepad | Recuperável | O usuário ainda pode utilizar teclado. |
| Áudio | Decisão pendente; recomendado recuperável | O jogo pode continuar em modo silencioso. |
| Configuração inexistente | Recuperável | O arquivo padrão pode ser criado. |
| Save inexistente | Recuperável | Representa uma instalação ou slot novo. |
| Falha real ao ler ou gravar save | Deve chegar ao fluxo do Memory Card | O jogo precisa diferenciar arquivo ausente de erro de permissão ou I/O. |

### Estrutura sugerida

- `SDLApp_PreInit()` deve retornar sucesso ou falha e o chamador deve respeitar o resultado.
- `SDLApp_FullInit()` deve inicializar os recursos por etapas.
- Cada etapa concluída deve ser registrada em estado interno.
- Em uma falha, somente os recursos já criados devem ser destruídos.
- `initialize_game()` deve retornar `bool`.
- O loop deve entrar no estado de jogo somente quando a inicialização completa for bem-sucedida.
- O encerramento deve ser seguro tanto pelo menu quanto pelo botão de fechar a janela.

## 3. Armazenamento portátil e Memory Card

### Objetivo confirmado

Todos os dados devem permanecer na pasta do jogo. O tratamento robusto não deve alterar esse comportamento nem usar diretórios alternativos do sistema.

### Funcionamento atual

O port do Memory Card converte os dois slots de PlayStation 2 nestes diretórios:

```text
data/saves/slot1/
data/saves/slot2/
```

O sistema implementa uma interface semelhante às funções `sceMc*` do PlayStation 2 e traduz as operações para arquivos comuns usando SDL.

### Falsos sucessos existentes

Em `src/port/sdk/sdk_libmc.c`, algumas operações informam sucesso mesmo quando a operação SDL pode ter falhado:

- `get_mc_root_path()` não valida falha de `SDL_asprintf()`;
- a criação de `data/`, `saves/` e dos slots não verifica o retorno de `SDL_CreateDirectory()`;
- `finalize_get_info()` sempre declara que existe um Memory Card formatado com aproximadamente 8 MB livres;
- `finalize_mkdir()` sempre retorna sucesso;
- `finalize_delete()` sempre retorna sucesso;
- `finalize_close()` retorna sucesso mesmo para um descritor inválido;
- `finalize_write()` não diferencia escrita completa de escrita parcial;
- `finalize_read()` não diferencia claramente fim de arquivo de erro real;
- `finalize_get_dir()` não trata falha de enumeração;
- erros diferentes de arquivo ausente podem acabar convertidos para `sceMcResNoEntry`.

No Windows, esses problemas não aparecem quando o jogo está em uma pasta gravável. Em Linux, macOS, mídia somente leitura ou pasta protegida, o jogo pode acreditar que o cartão está disponível e só apresentar erro posteriormente, ou até informar sucesso incorretamente.

### Verificação portátil recomendada

Antes de inicializar o jogo e declarar o Memory Card disponível:

1. Obter o caminho base com `SDL_GetBasePath()`.
2. Confirmar que o caminho não é `NULL`.
3. Criar `data/`.
4. Criar `data/saves/`.
5. Criar `data/saves/slot1/` e `data/saves/slot2/`.
6. Criar um arquivo temporário dentro de `data/`.
7. Gravar um conteúdo pequeno.
8. Fechar o arquivo.
9. Reabrir e confirmar o conteúdo.
10. Remover o arquivo temporário.

Esse teste verifica de fato criação, escrita, fechamento, leitura e remoção. Apenas conseguir criar ou encontrar o diretório não garante que saves possam ser gravados.

Se o teste falhar, o jogo deve mostrar uma mensagem clara antes de entrar no fluxo normal:

> O jogo não possui permissão para gravar na própria pasta. Extraia ou mova o jogo para uma pasta gravável e execute novamente.

Como o armazenamento portátil é requisito do fork, a recomendação é considerar essa falha fatal, sem fallback para outra pasta.

### Escrita segura de saves

Uma melhoria adicional seria tornar as gravações importantes atômicas:

1. Gravar em um arquivo temporário no mesmo diretório do save.
2. Confirmar que todos os bytes foram escritos.
3. Fechar o arquivo e verificar o status de I/O.
4. Substituir o arquivo final somente depois da gravação completa.

Isso reduz a possibilidade de corromper o save anterior se o programa for encerrado, faltar espaço ou ocorrer uma falha de escrita.

Essa alteração precisa ser estudada com cuidado porque o jogo original executa várias operações `sceMcOpen`, `sceMcWrite` e `sceMcClose`, e o port procura preservar o comportamento assíncrono esperado pelo fluxo original.

### Linux

O modelo portátil deve funcionar quando a pasta do jogo pertence ao usuário, por exemplo dentro da pasta pessoal. Pode falhar quando instalado em:

- `/usr/bin`;
- `/usr/local/bin`;
- `/opt` com proprietário administrativo;
- diretórios montados como somente leitura.

O teste inicial de escrita permite detectar esses casos sem alterar a política portátil.

### macOS

Em um bundle `.app`, `SDL_GetBasePath()` retorna por padrão a pasta de recursos interna, por exemplo:

```text
3SX.app/Contents/Resources/
```

Gravar dentro do bundle pode trazer problemas:

- execução direta a partir de DMG somente leitura;
- bundle instalado em pasta sem permissão de escrita;
- alteração do conteúdo de um bundle assinado;
- expectativa do usuário de encontrar `data/` ao lado do `.app`, e não dentro dele.

Uma alternativa compatível com a portabilidade é configurar no `Info.plist`:

```text
SDL_FILESYSTEM_BASE_DIR_TYPE = parent
```

Assim, o caminho base passa a ser o diretório que contém `3SX.app`, permitindo manter `data/` ao lado do aplicativo. Essa decisão ainda precisa ser aprovada e testada no macOS.

Executar diretamente de um DMG somente leitura deve produzir uma mensagem pedindo ao usuário que copie o aplicativo para uma pasta gravável.

## 4. Recursos de textura sem validação

### Recursos estruturais afetados

Os principais pontos estão em:

- `src/port/sdl/sdl_app.c`;
- `src/port/sdl/sdl_game_renderer.c`;
- `src/port/sdl/sdl_message_renderer.c`.

Recursos sem validação completa incluem:

- `screen_texture`;
- `cps3_canvas`;
- `message_canvas`;
- `knjsub_texture`;
- `knjsub_palette`;
- surfaces criadas a partir da memória do jogo;
- palettes dinâmicas;
- algumas operações de configuração de textura, surface e palette.

As funções da SDL podem retornar `NULL` por falta de memória, formato não suportado, dimensão inválida, renderer incompatível ou falha do backend gráfico.

### Problema mais direto

Em `create_screen_texture()`:

1. a textura anterior é destruída;
2. a nova textura é criada;
3. o retorno não é validado;
4. o código usa `screen_texture->w` e `screen_texture->h` posteriormente.

Se a criação falhar, o próximo frame pode acessar um ponteiro nulo.

### Tratamento sugerido para texturas de inicialização

As funções de inicialização devem retornar `bool`:

```c
bool SDLGameRenderer_Init(SDL_Renderer* renderer);
bool SDLMessageRenderer_Initialize(SDL_Renderer* renderer);
bool create_screen_texture(void);
```

Para cada recurso:

1. Criar o recurso.
2. Verificar se o ponteiro retornado é válido.
3. Aplicar scale mode, blend mode, palette ou outras propriedades.
4. Validar os retornos dessas operações quando forem relevantes.
5. Se qualquer etapa falhar, destruir o recurso parcial.
6. Registrar `SDL_GetError()` com nome, tamanho, formato e tipo do recurso.
7. Propagar a falha até `SDLApp_FullInit()`.

Como os canvases principais são essenciais, a falha deve encerrar a inicialização de maneira controlada.

### Tratamento durante redimensionamento

A textura antiga não deve ser destruída antes de a nova existir.

Fluxo recomendado:

```text
calcular novo tamanho
criar nova textura temporária
├── falhou: registrar erro e manter a textura anterior
└── funcionou:
    ├── configurar a nova textura
    ├── substituir o ponteiro ativo
    └── destruir a textura anterior
```

Esse tratamento permite sobreviver a uma falha transitória de resize sem perder imediatamente o alvo de renderização válido.

Também é recomendável evitar recriar a textura quando o novo tamanho for igual ao tamanho atual.

### Texturas e palettes dinâmicas do jogo

O cache já chama `fatal_error()` quando a criação final de determinada textura falha. Porém, faltam validações anteriores:

- conferir se o handle da textura está dentro do intervalo;
- conferir se o handle da palette está dentro do intervalo;
- validar `fl_texture->mem_handle` e o ponteiro de pixels;
- validar largura, altura e pitch;
- verificar o retorno de `SDL_CreateSurfaceFrom()`;
- verificar o retorno de `SDL_CreatePalette()`;
- verificar o retorno de `SDL_SetPaletteColors()`;
- verificar o retorno de `SDL_SetSurfacePalette()`;
- incluir texture handle, palette handle, formato e dimensões na mensagem fatal.

Para texturas do gameplay, continuar sem desenhar silenciosamente pode esconder corrupção de estado e produzir bugs difíceis de reproduzir. A recomendação é manter a falha fatal, mas com diagnóstico detalhado.

### Textura de mensagens

`knjsub_texture` é usada por mensagens e telas do jogo, inclusive fluxos relacionados ao Memory Card. A criação deveria ocorrer em um objeto temporário:

1. Criar a surface temporária.
2. Validar a surface.
3. Associar a palette.
4. Criar a nova textura.
5. Configurar scale e blend.
6. Somente depois substituir e destruir a textura anterior.

Em caso de falha, é preciso decidir entre:

- manter a textura anterior e registrar o erro;
- encerrar com erro fatal, evitando que uma tela importante fique invisível.

Como mensagens do Memory Card podem depender desse recurso, a opção fatal é a mais segura para evitar operação de save sem feedback visual.

## 5. Áudio, SPU, ADX e músicas por round

### Comportamento confirmado do jogo

Cada cenário ou personagem utiliza três músicas diferentes, relacionadas aos rounds. O jogo segue o sistema padrão de melhor de três, no qual o primeiro jogador a vencer dois rounds ganha a luta.

O comportamento atual de interromper, trocar, pausar, continuar ou encadear as faixas é controlado principalmente por:

- `BGM_Server()` em `src/sf33rd/Source/Game/sound/sound3rd.c`;
- `ADX_Stop()`;
- `ADX_Pause()`;
- `ADX_StartAfs()`;
- `ADX_EntryAfs()`;
- `ADX_StartSeamless()`.

O tratamento de erros proposto não deve modificar:

- qual música é escolhida em cada round;
- o momento em que uma música termina;
- transições entre rounds;
- fades;
- faixas seamless;
- músicas específicas de cenário ou personagem;
- regras de melhor de três.

### Dois sistemas de áudio distintos

O projeto possui pelo menos duas rotas principais:

- SPU/emulação do sistema de som para efeitos sonoros e sons internos;
- ADX/FFmpeg para músicas e faixas maiores.

Essas rotas abrem streams SDL separados e precisam de estados de inicialização independentes.

### Problema atual do ADX

`ADX_Init()` chama `SDL_OpenAudioDeviceStream()`, mas não verifica se o retorno é `NULL`.

Depois disso, várias funções presumem que `stream` é válido:

- `ADX_Stop()`;
- `ADX_Pause()`;
- `ADX_IsPaused()`;
- `ADX_SetOutVol()`;
- `ADX_Exit()`;
- consultas e alimentação do stream.

Além disso, `Init_sound_system()` ativa o bit de ADX em `system_init_level` mesmo que a abertura do dispositivo possa ter falhado. Como `BGM_Server()` usa esse bit para decidir se deve executar, o estado pode indicar uma inicialização que não ocorreu.

### Problema atual do SPU

`SPU_Init()` registra a falha de `SDL_OpenAudioDeviceStream()`, mas continua e chama funções usando o stream. Também é preciso validar:

- criação do mutex;
- criação do stream;
- retomada do dispositivo;
- operações que dependem do mutex ou stream.

Não existe atualmente um encerramento simétrico evidente para destruir explicitamente o stream e o mutex do SPU.

### Tratamento recomendado sem alterar músicas

- Fazer `ADX_Init()` retornar `bool`.
- Ativar o bit ADX em `system_init_level` somente se o stream tiver sido criado.
- Fazer as funções ADX tolerarem `stream == NULL`.
- Fazer `SPU_Init()` retornar `bool`.
- Manter estados separados para SPU e ADX.
- Registrar a falha apenas uma vez, evitando spam por frame.
- Se o áudio for considerado opcional, continuar em modo silencioso.
- Se o áudio for considerado obrigatório, mostrar erro e encerrar durante a inicialização.
- Chamar `Exit_sound_system()` no encerramento geral antes de destruir a SDL.
- Criar uma rotina simétrica de shutdown para o SPU.

### Recomendação

Considerar áudio recuperável e permitir execução silenciosa. Isso é mais robusto em máquinas sem dispositivo de áudio, sessões remotas, dispositivos desconectados ou backends incompatíveis.

Essa decisão ainda precisa ser aprovada. Se a experiência sem áudio não for considerada válida para este fork, a falha pode ser tratada como fatal sem modificar a lógica das músicas por round.

### Erros de carregamento de música

Há operações que retornam silenciosamente quando:

- não há slot livre para track;
- não é possível alocar buffer;
- `AFS_Open()` falha;
- uma leitura assíncrona falha;
- o decoder não consegue abrir uma faixa.

Esses casos deveriam ao menos produzir diagnóstico em Debug. Em Release, podem registrar somente um erro conciso em `data/error.log`, sem gerar os logs detalhados de desempenho usados nos testes.

## 6. Testes e logs existentes

### Escopo confirmado

Os testes e logs atuais foram criados para investigar:

- erros de textura;
- erros de áudio;
- stutter;
- possível atraso de input percebido na versão original do port.

Em Release, o jogo não gera os logs detalhados do modo de diagnóstico.

### Avaliação

Esse comportamento é adequado para o objetivo atual. Os diagnósticos detalhados podem afetar I/O e timing se forem executados constantemente, portanto mantê-los fora do fluxo Release reduz interferências.

Ainda é útil manter em Release:

- mensagens fatais claras;
- `data/error.log` para falhas reais;
- uma única mensagem por falha de subsistema;
- ausência de CSVs e telemetria por frame.

Não é necessário transformar imediatamente o projeto em uma suíte de testes automatizados para realizar os ajustes de robustez descritos neste documento.

## 7. AFS e fluxo de cópia de recursos

Embora não estivesse no pedido inicial, a análise da inicialização revelou pontos relacionados:

- `AFS_Init()` retorna `bool`, mas o retorno é ignorado;
- criação da fila de I/O assíncrono pode falhar;
- algumas alocações de tabela não são verificadas;
- a abertura do destino durante a cópia da ISO não é validada;
- a quantidade retornada por leitura e escrita não é conferida adequadamente;
- o fluxo pode usar um `SDL_IOStream*` nulo;
- falhas de escrita por pasta protegida podem ser confundidas com problemas da ISO.

Como o AFS contém os recursos essenciais do jogo, erros desse grupo devem ser fatais ou retornar ao diálogo de seleção com uma mensagem específica.

É importante diferenciar:

- ISO inválida;
- arquivo AFS ausente na ISO;
- checksum incorreto;
- falha para criar a pasta `resources/`;
- falta de permissão para gravar;
- disco cheio;
- escrita parcial;
- falha ao abrir o AFS já copiado.

## 8. Rollback e código online

O rollback e o netplay ficam fora do escopo deste fork.

Não é necessário investir em:

- save/load de estado do rollback;
- checksums online;
- matchmaking;
- P2P;
- sincronização de jogadores;
- histórico de input da sessão online;
- telas ou estatísticas de rede.

O código só deve ser considerado se interferir nos modos offline, por exemplo:

- macros de compilação afetando menus offline;
- inicialização desnecessária de dependências que cause falha no executável;
- chamadas executadas mesmo sem sessão online;
- alterações em variáveis globais usadas também pelo jogo local;
- impacto no tempo de frame dos modos offline.

Remover completamente o código e as dependências de rede pode ser uma tarefa futura separada. Não deve ser misturada com correções de SDL, textura, armazenamento ou áudio.

## 9. Divisão recomendada das futuras alterações

Para reduzir risco de regressão, os ajustes devem ser separados.

### Etapa A: inicialização e encerramento

Objetivos:

- tratar retornos SDL;
- propagar falhas até o loop principal;
- inicialização por etapas;
- cleanup parcial seguro;
- encerramento simétrico de áudio, renderer e arquivos.

Arquivos prováveis:

- `src/main.c`;
- `src/port/sdl/sdl_app.c`;
- `include/port/sdl/sdl_app.h`;
- arquivos de shutdown dos subsistemas envolvidos.

### Etapa B: armazenamento portátil e Memory Card

Objetivos:

- validar caminho base;
- realizar teste de escrita;
- propagar resultados reais das operações SDL;
- evitar falsos sucessos do Memory Card;
- definir comportamento específico para Linux e macOS.

Arquivos prováveis:

- `src/port/paths.c`;
- `src/port/paths.h`;
- `src/port/sdk/sdk_libmc.c`;
- possivelmente `src/main.c` para o teste inicial;
- `cmake/Info.plist`, caso seja aprovado usar o diretório pai do `.app`.

### Etapa C: renderer e texturas

Objetivos:

- validar canvases, surfaces, palettes e texturas;
- recriação segura durante resize;
- mensagens de erro com handles, formatos e dimensões;
- impedir acesso a ponteiros nulos ou índices inválidos.

Arquivos prováveis:

- `src/port/sdl/sdl_app.c`;
- `src/port/sdl/sdl_game_renderer.c`;
- `include/port/sdl/sdl_game_renderer.h`;
- `src/port/sdl/sdl_message_renderer.c`;
- `include/port/sdl/sdl_message_renderer.h`.

### Etapa D: áudio robusto

Objetivos:

- separar estado de ADX e SPU;
- validar streams e mutex;
- decidir entre áudio obrigatório ou modo silencioso;
- shutdown simétrico;
- preservar integralmente a lógica das músicas por round.

Arquivos prováveis:

- `src/port/sound/adx.c`;
- `src/port/sound/adx.h`;
- `src/port/sound/spu.c`;
- `include/port/sound/spu.h`;
- `src/port/sound/emlShim.c`;
- `src/sf33rd/Source/Game/sound/sound3rd.c`;
- `src/sf33rd/Source/Game/sound/sound3rd.h`;
- `src/main.c` para o encerramento geral.

### Etapa E: cópia e abertura do AFS

Objetivos:

- validar abertura, leitura e escrita;
- diferenciar erro da ISO de erro no destino;
- remover arquivo incompleto em falha;
- impedir inicialização com AFS inválido.

Arquivos prováveis:

- `src/port/resources.c`;
- `src/port/resources.h`;
- `src/port/io/afs.c`;
- `src/port/io/afs.h`;
- `src/main.c`.

## 10. Decisões pendentes

Antes de implementar, é necessário decidir:

1. Falha ao abrir áudio deve permitir modo silencioso ou encerrar o jogo?
2. No macOS, `data/` deve ficar dentro do `.app` ou ao lado do `.app`?
3. A verificação de escrita portátil deve ocorrer sempre ou somente na primeira execução?
4. Falha de uma textura dinâmica deve encerrar imediatamente ou tentar manter o último recurso válido quando possível?
5. O port do Memory Card deve representar erros reais com os códigos `sceMcRes*` esperados pelo jogo ou usar uma mensagem externa antes de iniciar o jogo?
6. Os saves devem futuramente usar substituição atômica para proteger o arquivo anterior?
7. Erros essenciais em Release devem ser registrados somente em `data/error.log` ou também exibidos em uma caixa de mensagem?
8. A remoção completa do código e dependências online será feita futuramente ou eles permanecerão compilados, porém inacessíveis?

## 11. Ordem sugerida de prioridade

1. Inicialização SDL e propagação de erros fatais.
2. Teste real de escrita na pasta portátil.
3. Correção dos falsos sucessos do Memory Card.
4. Validação das texturas estruturais.
5. Recriação segura de textura durante resize.
6. Tratamento de falha de ADX e SPU sem alterar a lógica dos rounds.
7. Encerramento simétrico dos subsistemas.
8. Robustez da cópia e abertura do AFS.
9. Testes específicos em Linux.
10. Testes específicos em macOS e decisão final sobre a localização de `data/`.
11. Avaliação futura da remoção física do código online.
