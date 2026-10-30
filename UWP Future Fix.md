# UWP Future Fix

## Estado e decisão de parada — 30/10/2026

Investigação de engasgos eventuais suspensa por decisão do usuário. O projeto
está satisfatório para uso no Xbox One S Dev Mode: foram relatadas partidas
completas fluidas e um gameplay posterior de aproximadamente 40 segundos com
atrasos muito eventuais. Isso não significa ausência de todos os picos.

Trabalho na branch UWP, preservando o comportamento desktop. O checkpoint
estável publicado anteriormente foi e66c1dbc2a54a7bedc953cada55f8e4f336651c0.
As modificações de diagnóstico e esta documentação posteriores a ele estavam
locais; este documento não representa um novo commit, tag ou envio ao remoto.
O usuário executa todas as builds, compilações de shaders e testes no console.

## Melhorias que devem ser preservadas

- Apresentação UWP corrigida: canvas interno 1280×720, conteúdo 4:3 960×720,
  swapchain ajustada à janela e DXGI_SCALING_STRETCH para ocupar a altura da
  saída 1920×1080 com barras laterais. Houve tentativas anteriores de fullscreen
  e retângulo de destino que não resolveram a superfície pequena centralizada.
- Direct3D 11, feature level 10_1 e shader avançado ps_4_0. Não confundir versão
  da API, feature level e perfil do shader.
- VSync, cuja ausência de tearing foi confirmada pelo usuário.
- Reaproveitamento de texturas indexadas e preservação de recursos necessários
  aos desenhos já enfileirados, reduzindo criação e descarte de texturas.
- Correção de controle duplicado: UWP passou a usar WGI sem concorrer com XInput.
  O usuário testou dois controles, um com cabo e outro sem fio, sem espelhamento.
- Instrumentação detalhada de input foi retirada: as leituras medidas eram
  rápidas, e a investigação passou para o trabalho posterior do jogo.
- Builds automáticas do GitHub foram desativadas anteriormente para esse fluxo.

## Sintoma investigado

Primeiro uso de certos golpes na primeira luta podia gerar um quadro próximo
de 100 ms. Repetir o golpe na mesma luta, ou voltar à seleção e repetir a luta
sem fechar o processo, frequentemente não reproduzia o atraso. Não era uma
lentidão contínua; o usuário relatou input, som e música normais nas partidas.

Foram usados Hugo contra Yun e outras combinações, incluindo Ken contra Ryu.
Alguns testes mudaram personagens ou identificador de cenário entre lutas;
essas sessões não devem ser tratadas como comparações perfeitamente idênticas.

## Hipóteses e instrumentações realizadas

### Renderização, input e custo geral

As medições separaram trabalho do jogo, tarefas, processamento dos jogadores,
efeitos, atualizações de textura, geometria, cópia e apresentação. Os grandes
picos estudados ocorreram frequentemente no trabalho do jogo antes da
apresentação. Isso não exclui outros picos de GPU/apresentação em outros frames.
Logs de input não medem latência física do botão até a TV.

O logger UWP foi convertido em worker assíncrono, com fila limitada, descarte
contabilizado, teto de arquivo e flush medido. Isso reduziu gravações síncronas
na thread principal, mas não demonstrou que toda interferência de I/O desapareceu.

### Som e buffers

O usuário já havia trabalhado com buffer de música no PC e sugeriu investigar
efeitos sonoros no primeiro acerto. Foram instrumentados upload SPU, vozes,
espera de lock e comandos de áudio. A implementação usa RAM SPU residente de
2 MiB; os resultados não sustentaram afirmar que cada golpe lê um arquivo de
som do disco. Houve esperas de áudio em algumas fases, sem comprovação de que
explicavam o engasgo estudado. Não foi validada uma nova solução geral de
pré-carregamento de todos os efeitos sonoros nesta investigação.

### Caixas de colisão

Foram medidos testes e fila de colisões. Os resultados observados foram baixos:
aproximadamente 1–5 testes e fila de 2, sem evidência de desenho repetido de
hitboxes ou custo suficiente para explicar os grandes picos. A instrumentação
detalhada das caixas foi removida a pedido do usuário. A hipótese não recebeu
suporte nos testes coletados; isso não constitui prova para todos os estados.

### Ataques, animações, dano e efeitos — versões 14 a 16

O rastreamento passou por Player_attack, handlers, rotinas adicionais por
personagem, inicialização de animação, reação de dano e callbacks de efeitos.
As durações são inclusivas: não somar pai e filho como trabalho independente.
Tempos medem passagem de relógio, incluindo eventuais esperas ou preempção.

- Versão 14: aproximadamente 83,51 ms na rotina adicional 16 do Hugo, que
  seleciona Att_HADOUKEN2. O nome compartilhado não significa que Hugo emita
  o projétil do Ken; é uma rotina genérica usada por diferentes ataques.
- Outras sessões localizaram picos em reação de dano e callbacks de efeitos,
  sem demonstrar que todos têm a mesma causa.
- Versão 16: frame 2048 com char_init_table_lookup de 77,094 ms,
  char_init_prepare de 77,117 ms e effect_callback de 77,324 ms; quadro de
  aproximadamente 92,8 ms. A expressão medida consulta um offset de tabela
  e calcula um endereço. Ela não comprova leitura de disco nem page fault.

O efeito 02 usa _ef01_char_table, array estático compilado em
src/bin2obj/char_table.c. Não atribuir essa tabela ao carregamento do AFS.

### Aquecimento experimental — versão 17

Foi implementada a sugestão de antecipar o primeiro acesso. A função
uwp_warm_hit_effect_table faz leituras volatile de todo o array
_ef01_char_table, com tamanho obtido por sizeof e checksum. Game2_1 a chama
uma vez por processo no primeiro processamento da luta. Uma demonstração
automática também pode executá-la antes da partida do usuário.

Não há golpe real, objeto temporário, áudio ou simulação oculta. É um
aquecimento dos dados da tabela de efeito; não exercita todas as rotinas de
ataque, shaders ou recursos de personagem. Não há garantia de residência
permanente das páginas. O aquecimento permanece habilitado no ponto de parada.

A leitura antecipada levou aproximadamente 47,7–48,6 ms nos testes. Mesmo
assim, a versão 17 ainda apresentou:

- Hugo: frame 1716 de 111,3 ms; rotina adicional de ataque de 91,9 ms;
  impacto somente no frame 1735.
- Ken: frame 1576 de 100,1 ms; rotina adicional de 88,9 ms; impacto no
  frame 1603. Segundo Hadouken e luta repetida sem pico equivalente registrado.

Portanto, aquecer apenas essa tabela não resolveu consistentemente a pausa.

### Entrada compartilhada e CPU — versão 18

Confirmado no código: pl06_extra_attack seleciona Att_HADOUKEN2; no Ken,
pl11_extra_attack seleciona Att_HADOUKEN, que chama Att_HADOUKEN2.
O caminho inclui duas seleções por tabelas de ponteiros de função.

Foi adicionado attack_entry_trace para início 16/0 de Hugo e Ken, com marcos:
entrada do wrapper do personagem, função selecionada, entrada compartilhada,
fim do ajuste de posição e fim da inicialização da animação. Os marcos são
milissegundos desde o início, não durações independentes. -1 significa ausente.
GetThreadTimes tenta obter CPU da thread; cpu_valid sinaliza disponibilidade.
A resolução pode ser grosseira e a própria medição tem custo.

Um conflito de compilação foi corrigido: windows.h deve preceder common.h,
cuja macro BOOL(...) interfere em declarações do SDK. WIN32_LEAN_AND_MEAN e
NOMINMAX limitam inclusões e macros do Windows.

No teste final da versão 18, o usuário não percebeu engasgo no primeiro uso.
Não houve attack_entry_trace: o registro só sai para chamadas de pelo menos
1 ms. Isso impede concluir tempos exatos ou CPU daquele início. Não foi
registrado o pico anterior de aproximadamente 90 ms na rotina adicional.

Correção de interpretação do relato anterior: no frame 2005, os 17,387 ms
eram de player_routine, e 2,107 ms de active_state; não eram 17,4 ms medidos
em attack_extra_routine. No frame 2013, attack_extra_routine foi 5,719 ms.
Impactos nos frames 2028, 2187 e 3536. Houve outro callback de efeito de
54,319 ms no frame 2043. O teste não prova eliminação definitiva do problema.

## Situação das hipóteses

Primeiro acesso a código/dados e interrupção/agendamento da thread continuam
possíveis. Ainda não se isolou uma instrução responsável. Não há demonstração
de compilação de shader, defeito SDL3, saturação contínua de CPU ou acesso ao
AFS por golpe como causa dessa pausa. A localização em uma função compartilhada
não prova que compartilhar a função seja um erro.

Limitações do UWP e do hardware Xbox One S podem contribuir, mas isso é
hipótese. O usuário não possui Xbox Series para comparação; não afirmar que
o problema inexiste ou estará resolvido nesse hardware mais forte.

Executar um ataque antecipadamente é uma possibilidade não implementada.
Chamar a rotina com WORK/PLW fictício ou temporariamente modificar o personagem
real pode causar efeitos colaterais em animação, combate e estado global.

## Suspensão das gravações de diagnóstico

include/port/uwp_diagnostics.h centraliza SF3_UWP_DIAGNOSTIC_DISK_LOGGING=0.
No UWP, o worker de arquivo não é iniciado, uwp_render_check.log e
uwp_resource_check.log não são criados/atualizados, e DebugLog_Init não habilita
arquivos de sessão/CSV mesmo que a configuração persistida peça debug.
Mensagens SDL com prefixo [UWP_ são descartadas pela saída de diagnóstico.
Os hooks de medição permanecem; amostras de combate em memória são limitadas
e descartadas ao fim do frame sem exportação. Os arquivos antigos não são
apagados e não representam uma nova sessão depois desta alteração.

Isso suspende I/O de diagnóstico, não as gravações funcionais do jogo, como
configurações e saves, nem necessariamente relatórios de erros excepcionais.
Não se afirmou que o tamanho dos logs causava os engasgos; a finalidade é
retirar essa atividade do uso normal. A validação dessa alteração no console
ainda depende da próxima build e teste do usuário.

## Retomada futura, se o usuário decidir

Preservar primeiro este estado e os logs de referência. Reativar explicitamente
o diagnóstico e registrar também os primeiros ataques rápidos, com orçamento
limitado, para distinguir ausência de amostra de ausência de execução. Comparar
passagem de relógio e CPU, delimitando chamadas indiretas antes de adicionar
outros aquecimentos. Repetir personagens, cenário, golpe e sequência de ações,
comparando primeira execução após abrir o app, repetição na mesma luta e nova
luta sem reiniciar. No momento, nenhuma dessas novas etapas está em andamento.
