# Ajuste fino 2

## Objetivo desta rodada

Esta rodada deve reforcar os caminhos de erro e o ciclo de vida dos recursos sem alterar as regras do jogo, o comportamento dos rounds, a criacao automatica dos Memory Cards, o cache fixo de sprites ou o carregamento assincrono de audio.

As correcoes estao organizadas por prioridade. Cada etapa deve ser validada separadamente para facilitar a identificacao de regressoes.

## Prioridade 1 - Ciclo de vida assincrono do AFS

Arquivos principais:

- `src/port/io/afs.c`
- Cabecalhos do AFS, caso seja necessario expor novos estados internos.

Problemas identificados:

- O `SDL_AsyncIO` usado por uma leitura pode deixar de ser referenciado sem passar por `SDL_CloseAsyncIO`.
- `AFS_Close` libera a solicitacao antes de o resultado assincrono do fechamento chegar.
- Um slot pode ser reutilizado enquanto ainda existe um resultado antigo apontando para ele.
- Uma resposta atrasada pode modificar o estado de uma nova leitura que reutilizou o mesmo slot.

Ajustes:

1. Adicionar estados explicitos para a solicitacao, por exemplo `FREE`, `READING`, `CLOSING`, `COMPLETED` e `FAILED`.
2. Manter o handle `SDL_AsyncIO` valido ate a confirmacao do fechamento.
3. Solicitar o fechamento depois de uma leitura concluida ou cancelada.
4. Liberar o slot somente ao receber o resultado correspondente ao fechamento.
5. Associar uma geracao ou identificador a cada reutilizacao do slot, impedindo que respostas antigas alterem solicitacoes novas.
6. Preservar o funcionamento assincrono e evitar esperas bloqueantes no fluxo de audio.
7. Garantir que `AFS_Stop` e `AFS_Close` sejam seguros quando chamados mais de uma vez.

Criterios de validacao:

- Alternar musicas por muitos rounds sem aumento continuo de handles.
- Interromper uma musica durante o carregamento e iniciar outra imediatamente.
- Sair para o menu e iniciar novas partidas repetidamente.
- Confirmar que nenhuma resposta antiga modifica a musica atual.

## Prioridade 2 - Dialogo e copia dos recursos da ISO

Arquivo principal:

- `src/port/resources.c`

Problemas identificados:

- O filtro passado ao dialogo assincrono da SDL e uma variavel local e pode ficar invalido depois que a funcao retorna.
- O callback do dialogo pode executar em outra thread e escreve diretamente em `flow_state` e `error`.
- O cancelamento do dialogo e tratado da mesma maneira que uma falha.
- A leitura do ultimo bloco da ISO pode retornar um setor completo quando restam menos bytes do arquivo.

Ajustes:

1. Manter o filtro do dialogo em armazenamento `static const`.
2. Nao modificar o estado principal diretamente no callback de outra thread.
3. Publicar o resultado por evento SDL ou por uma estrutura sincronizada e consumir o resultado na thread principal.
4. Diferenciar cancelamento, erro do dialogo, ISO invalida e recurso ausente.
5. Validar o retorno de `iso9660_iso_seek_read`.
6. Gravar e incluir no checksum apenas `bytes_to_read`, mesmo quando a biblioteca le um setor completo.
7. Impedir underflow de `bytes_remaining`.
8. Remover o arquivo de destino incompleto quando a copia ou o checksum falhar.
9. Manter nas mensagens o caminho completo do recurso que causou o erro.

Criterios de validacao:

- Cancelar o dialogo sem entrar em repeticao involuntaria.
- Selecionar um arquivo que nao seja uma ISO valida.
- Selecionar uma ISO sem `SF33RD.AFS`.
- Simular falha de leitura, falta de espaco e checksum incorreto.
- Confirmar que nao permanece um `SF33RD.AFS` parcial.

## Prioridade 3 - Integridade do Memory Card

Arquivo principal:

- `src/port/sdk/sdk_libmc.c`

Arquivos de fluxo que devem ser apenas conferidos:

- `src/sf33rd/Source/PS2/mc/mcsub.c`
- Demais chamadores das funcoes `sceMc*`.

Problemas identificados:

- Uma leitura menor que o tamanho solicitado pode ser retornada como sucesso.
- Uma gravacao interrompida pode deixar o arquivo anterior parcialmente sobrescrito.
- Erros diferentes de exclusao podem ser convertidos incorretamente em arquivo inexistente.

Ajustes:

1. Para saves e replays de tamanho definido, considerar sucesso somente quando a leitura entregar exatamente o tamanho solicitado.
2. Converter leitura curta em um erro compativel com o contrato esperado pelo jogo.
3. Nao alterar a verificacao que permite iniciar sem um Memory Card criado.
4. Preservar a criacao automatica do MC1 e MC2 no momento em que o jogo tenta salvar.
5. Estudar gravacao atomica com arquivo temporario, fechamento confirmado e substituicao do destino.
6. Se a gravacao temporaria falhar, preservar o save anterior.
7. Mapear de maneira distinta arquivo ausente, permissao negada, dispositivo cheio e erro de I/O quando a API do jogo permitir.

Criterios de validacao:

- Iniciar sem as pastas de MC e salvar normalmente.
- Salvar e carregar no MC1 e MC2.
- Salvar e carregar replays no MC1 e MC2.
- Testar arquivo truncado, arquivo somente leitura e falta de espaco.
- Confirmar que uma falha de gravacao nao elimina o save valido anterior.

## Prioridade 4 - Robustez do pipeline ADX

Arquivos principais:

- `src/port/sound/adx.c`
- `src/port/sound/adx.h`, somente se o contrato interno precisar mudar.

Operacoes que precisam de validacao:

- `avcodec_find_decoder`
- `avcodec_alloc_context3`
- `avcodec_open2`
- `av_parser_init`
- `swr_alloc_set_opts2`
- `swr_init`
- `av_packet_alloc`
- `av_frame_alloc`
- `av_samples_alloc`
- `swr_convert`
- `SDL_PutAudioStreamData`

Ajustes:

1. Fazer a inicializacao do pipeline retornar sucesso ou falha.
2. Desmontar com seguranca pipelines parcialmente inicializados.
3. Nao desreferenciar codec, contexto, parser, packet, frame ou resampler nulos.
4. Validar quantidade de samples convertidos e tamanho enviado ao stream.
5. Interromper somente a faixa afetada quando ocorrer erro de decodificacao.
6. Registrar um erro critico conciso em Release, incluindo faixa ou recurso relacionado.
7. Preservar o carregamento assincrono e o comportamento de uma musica diferente por round.

Criterios de validacao:

- Jogar uma partida completa com as tres musicas do conjunto.
- Repetir transicoes de round, continue, pause, retorno ao menu e nova partida.
- Testar dados ADX invalidos ou incompletos.
- Confirmar ausencia de travamento, audio residual e microstuttering novo.

## Prioridade 5 - Preservacao dos erros reais de textura

Arquivos principais:

- `src/port/sdl/sdl_app.c`
- `src/port/sdl/sdl_message_renderer.c`
- `src/port/sdl/sdl_game_renderer.c`, se o mesmo padrao estiver presente para paletas ou texturas.

Problema identificado:

- Funcoes de destruicao podem ser chamadas com um recurso nulo logo depois de uma falha de criacao, substituindo a mensagem SDL original por um erro generico.

Ajustes:

1. Capturar `SDL_GetError()` imediatamente depois da operacao que falhou.
2. Destruir apenas recursos validos.
3. Incluir na mensagem o tipo do recurso, dimensoes, formato e contexto disponivel.
4. Manter falhas essenciais como fatais.
5. Evitar qualquer alocacao adicional no caminho normal de renderizacao.

Criterios de validacao:

- Simular falha na criacao da textura principal.
- Solicitar dimensoes invalidas em um teste controlado de Debug.
- Confirmar que a mensagem mostra a causa original da SDL.

## Prioridade 6 - Validacao estrutural do AFS

Arquivo principal:

- `src/port/io/afs.c`

Problemas identificados:

- Algumas leituras e operacoes de seek nao verificam retorno.
- A quantidade de entradas nao e comparada com o tamanho do arquivo.
- A leitura de nomes nao possui um limite explicito para o buffer de destino.
- Calculos de offsets podem sofrer underflow com uma tabela invalida.

Ajustes:

1. Validar assinatura, quantidade de entradas, offsets e tamanhos antes de alocar.
2. Proteger multiplicacoes usadas para calcular tamanhos de alocacao.
3. Passar a capacidade do destino para a funcao que le strings.
4. Interromper nomes longos sem ultrapassar o buffer e garantir terminador nulo.
5. Verificar todo `SDL_ReadIO`, `SDL_SeekIO` e fechamento relevante.
6. Rejeitar entradas fora dos limites do arquivo.
7. Informar na mensagem o caminho do AFS invalido.

Criterios de validacao:

- AFS truncado, assinatura invalida e tabela com numero excessivo de entradas.
- Nome sem terminador e offset fora do arquivo.
- AFS original valido sem mudanca de desempenho perceptivel.

## Prioridade 7 - Armazenamento portatil

Arquivos principais:

- `src/port/paths.c`
- `src/port/paths.h`, se houver mudanca no contrato interno.

Ajustes:

1. Substituir o nome fixo do arquivo de teste de escrita por um nome exclusivo por processo ou tentativa.
2. Criar o teste somente dentro da pasta portatil escolhida.
3. Remover o teste em todos os caminhos de saida possiveis.
4. Manter todos os dados ao lado do executavel no Windows e Linux e ao lado do bundle no macOS.
5. Exibir caminho completo e erro do sistema quando a pasta nao permitir escrita.
6. Nao redirecionar silenciosamente saves para AppData, HOME ou outro diretorio.

Criterios de validacao:

- Executar duas instancias simultaneamente.
- Testar pasta gravavel, somente leitura e caminho com caracteres Unicode.
- Copiar a pasta completa para outro computador e carregar o mesmo save.

## Prioridade 8 - Remocao das interfaces de Netplay restantes

Arquivos principais:

- `src/args.c`
- `src/configuration.h`
- Arquivos de Debug que ainda exponham metricas exclusivamente online.

Ajustes:

1. Remover argumentos P2P e matchmaking que nao possuem efeito util neste fork.
2. Remover configuracoes de Netplay sem consumidores no jogo local.
3. Remover colunas de log exclusivamente online quando permanecerem sempre zeradas.
4. Nao alterar bibliotecas ou codigo antigo somente por existirem no repositorio, a menos que sejam compilados, inicializados ou afetem o jogo local.
5. Conferir novamente menus, replays e inicializacao sem definir suporte online.

Criterios de validacao:

- Modos Arcade, Versus, Training e Replay continuam acessiveis.
- Nenhuma thread, socket ou servico online e iniciado.
- Argumentos removidos nao aparecem na ajuda da aplicacao.

## Logs de Release e Debug

Ajustes gerais:

- Manter os logs detalhados atuais somente em Debug ou quando `--debug-mode` for solicitado.
- Em Release, gravar apenas erros criticos de inicializacao, recursos, armazenamento, renderizacao, audio e I/O.
- Evitar escrita continua de log durante uma partida normal.
- Cada erro critico deve conter subsistema, operacao, arquivo relacionado quando houver e mensagem original da biblioteca.
- Se a propria pasta portatil nao puder ser escrita, mostrar a mensagem em tela mesmo que o log nao possa ser criado.

## Ordem de implementacao sugerida

1. Ciclo de vida assincrono do AFS.
2. Dialogo e copia da ISO.
3. Integridade do Memory Card.
4. Pipeline ADX.
5. Diagnosticos de textura.
6. Parser AFS.
7. Armazenamento portatil.
8. Limpeza das interfaces de Netplay.
9. Revisao dos logs de Release e Debug.

## Restricoes desta rodada

- Nao alterar regras de gameplay ou o sistema de melhor de tres rounds.
- Nao alterar a selecao das tres musicas por cenario/personagem.
- Nao remover o cache fixo de sprites.
- Nao voltar a criar e destruir sprites continuamente.
- Nao transformar os carregamentos assincronos em operacoes bloqueantes.
- Nao exigir um Memory Card existente para iniciar o jogo.
- Nao mover dados portateis para pastas do sistema operacional.
- Nao implementar ou reparar Netplay e rollback.
- Nao incluir assets proprietarios no repositorio ou nos pacotes de build.

