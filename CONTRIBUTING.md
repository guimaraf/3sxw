# Contributing

Please read this before making changes to the repository.

## Project direction

3SXW is a hard fork of 3SX focused on a practical local/offline desktop port of the PlayStation 2 version of Street Fighter III: 3rd Strike.

This fork intentionally keeps:
- local save and config files inside the project/game folder;
- replay support intended to work like the console experience in practice;
- Gill unlocked from the start;
- desktop quality-of-life features;
- platform-specific adaptations required for modern hardware.

This fork does **not** currently prioritize:
- broad refactors for code elegance alone;
- online/netplay work;
- removing intentional fork-specific behavior;
- turning the project into a strict 1:1 restoration of original PS2 code structure.

## Before changing code

- Read the main [README.md](README.md).
- Read the relevant build guide in [docs/buildWindows.md](docs/buildWindows.md), [docs/buildLinux.md](docs/buildLinux.md), or [docs/buildMac.md](docs/buildMac.md).
- Understand the intent of the fork before proposing cleanups or behavior changes.

## Legal and repository safety

- Do not add original game assets, audio, artwork, logos, BIOS, firmware, dumps, extracted files, or other proprietary materials to the repository.
- Do not open pull requests that include copyrighted game content or official branding assets.
- The project must continue to require each user to provide their own legally obtained original copy of the game.
- Keep public documentation aligned with the legal notice in [README.md](README.md).

## Contribution expectations

When contributing, prefer:
- small, surgical changes;
- preserving established fork behavior;
- fixes for real functional issues;
- improvements to local usability, save flow, replay flow, and portability;
- documentation updates when paths, scripts, or workflows change.

Please avoid:
- unnecessary large refactors;
- changing intentional desktop behavior without a clear reason;
- prioritizing online-related work over local/offline goals;
- removing fork-specific improvements just to match upstream.

## Build and testing

- Windows is currently the main validated platform.
- Linux and macOS build support exists, but may need more validation.
- If you change build scripts, install paths, packaging, or documentation, update the related docs and workflows together.

Build scripts:
- Root shortcut: `build-deps.sh`
- Platform scripts: `scripts/build/`

## Coding notes

- Follow the style already used in the surrounding code.
- Do not refactor reverse-engineered code unless there is a practical payoff.
- Preserve working behavior unless the change is intentional and justified.
- If a change affects save, replay, local paths, resources, or packaging, treat it as high impact and review it carefully.

---

# Contribuicao

Leia este arquivo antes de fazer alteracoes no repositorio.

## Direcao do projeto

3SXW e um hard fork de 3SX focado em um port pratico para desktop, local/offline, da versao de PlayStation 2 de Street Fighter III: 3rd Strike.

Este fork mantem intencionalmente:
- saves e configuracoes locais dentro da pasta do projeto/jogo;
- suporte a replay pensado para funcionar, na pratica, como a experiencia de console;
- Gill liberado desde o inicio;
- recursos de qualidade de vida para desktop;
- adaptacoes de plataforma necessarias para hardware moderno.

Este fork **nao** prioriza atualmente:
- refatoracoes amplas apenas por elegancia;
- trabalho focado em online/netplay;
- remocao de comportamentos intencionais do fork;
- transformar o projeto em uma restauracao estrita 1:1 da estrutura original de codigo do PS2.

## Antes de alterar codigo

- Leia o [README.md](README.md).
- Leia o guia de build relevante em [docs/buildWindows.md](docs/buildWindows.md), [docs/buildLinux.md](docs/buildLinux.md) ou [docs/buildMac.md](docs/buildMac.md).
- Entenda a intencao do fork antes de propor limpezas ou mudancas de comportamento.

## Seguranca juridica e do repositorio

- Nao adicione ao repositorio assets originais do jogo, audios, artes, logos, BIOS, firmware, dumps, arquivos extraidos ou outros materiais proprietarios.
- Nao abra pull requests contendo conteudo protegido do jogo ou assets oficiais de branding.
- O projeto deve continuar exigindo que cada usuario forneca sua propria copia original obtida legalmente.
- Mantenha a documentacao publica alinhada com o aviso legal em [README.md](README.md).

## Expectativas de contribuicao

Ao contribuir, prefira:
- alteracoes pequenas e cirurgicas;
- preservar o comportamento ja estabelecido pelo fork;
- correcoes para problemas funcionais reais;
- melhorias de usabilidade local, fluxo de save, fluxo de replay e portabilidade;
- atualizar a documentacao quando paths, scripts ou workflows mudarem.

Evite:
- refatoracoes grandes sem necessidade;
- mudar comportamento intencional de desktop sem motivo claro;
- priorizar trabalho ligado a online acima dos objetivos locais/offline;
- remover melhorias do fork apenas para se aproximar do upstream.

## Build e testes

- Windows e atualmente a principal plataforma validada.
- Linux e macOS possuem suporte de build, mas ainda podem precisar de mais validacao.
- Se voce alterar scripts de build, caminhos de install, empacotamento ou documentacao, atualize os docs e workflows relacionados no mesmo conjunto de mudancas.

Scripts de build:
- Atalho na raiz: `build-deps.sh`
- Scripts por plataforma: `scripts/build/`

## Notas de codigo

- Siga o estilo ja usado no codigo ao redor.
- Nao refatore codigo de engenharia reversa sem ganho pratico claro.
- Preserve comportamento funcional existente, a menos que a mudanca seja intencional e justificada.
- Se a alteracao afetar save, replay, paths locais, resources ou empacotamento, trate como mudanca de alto impacto e revise com cuidado.
