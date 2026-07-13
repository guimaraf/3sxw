# Build Guide: Linux

This guide explains how to build **3SX (Street Fighter III: 3rd Strike)** on Linux distributions based on Debian or Ubuntu.

## 1. Install required packages

Before building, install the base toolchain and libraries:

```bash
sudo apt-get update
sudo apt-get install -y $(cat tools/requirements-ubuntu.txt)
```

## 2. Prepare `third_party`

If the build complains about missing `.a` libraries, headers, or dependencies, check `third_party/` first.

Platform-specific build scripts are stored in `scripts/build/`.

For Linux:

```bash
bash scripts/build/build-deps-linux.sh
```

If you prefer the root shortcut, you can also run:

```bash
bash build-deps.sh
```

These scripts prepare FFmpeg, SDL3, libcdio, minizip-ng, and tf-psa-crypto in `third_party/`.

## 3. Configure and build

Configure the project:

```bash
CC=clang CXX=clang++ cmake -B build -DCMAKE_BUILD_TYPE=Release
```

Build the project:

```bash
cmake --build build --parallel
```

> [!NOTE]
> If nothing changed since the last successful build, Ninja may report `no work to do.`. That is expected.

## 4. Install the final output

Install the final output:

```bash
cmake --install build --prefix build/application
```

> [!NOTE]
> `cmake --install` does not rebuild the project. It only reuses artifacts already generated in `build/`.

## 5. Add game assets

> [!IMPORTANT]
> The game will not run correctly without the original assets.
> You must use your own legally obtained original copy of the game.
> This repository is not affiliated with or endorsed by Capcom and does not include proprietary game assets.

After installation:

1. Go to `build/application/`
2. Create a folder named `resources`
3. Place `SF33RD.AFS` inside it

Final path:

```text
build/application/resources/SF33RD.AFS
```

## Native hardware validation status

Compilation and runtime validation on real Linux hardware are still pending. The build scripts have been reviewed statically, but the complete game flow must be tested on a native Linux machine before this platform is considered validated.

Pending checks include startup, ISO selection and cancellation, `SF33RD.AFS` extraction, portable writes, `error.log`, MC1/MC2 saves, replays, audio, input, window behavior, shutdown and shared-library resolution.

---

# Guia de Compilacao: Linux

Este guia explica como compilar o **3SX (Street Fighter III: 3rd Strike)** em distribuicoes Linux baseadas em Debian ou Ubuntu.

## 1. Instale os pacotes necessarios

Antes de compilar, instale o toolchain e as bibliotecas base:

```bash
sudo apt-get update
sudo apt-get install -y $(cat tools/requirements-ubuntu.txt)
```

## 2. Prepare `third_party`

Se o build reclamar de bibliotecas `.a`, headers ou dependencias faltando, verifique `third_party/` primeiro.

Os scripts especificos por plataforma ficam em `scripts/build/`.

Para Linux:

```bash
bash scripts/build/build-deps-linux.sh
```

Se preferir usar o atalho da raiz, voce tambem pode rodar:

```bash
bash build-deps.sh
```

Esses scripts preparam FFmpeg, SDL3, libcdio, minizip-ng e tf-psa-crypto em `third_party/`.

## 3. Configure e compile

Configure o projeto:

```bash
CC=clang CXX=clang++ cmake -B build -DCMAKE_BUILD_TYPE=Release
```

Compile o projeto:

```bash
cmake --build build --parallel
```

> [!NOTE]
> Se nada mudou desde o ultimo build bem-sucedido, o Ninja pode responder `no work to do.`. Isso e esperado.

## 4. Instale a saida final

Instale a saida final:

```bash
cmake --install build --prefix build/application
```

> [!NOTE]
> `cmake --install` nao recompila o projeto. Ele apenas reutiliza os artefatos ja gerados em `build/`.

## 5. Adicione os assets do jogo

> [!IMPORTANT]
> O jogo nao vai funcionar corretamente sem os assets originais.
> Voce deve usar sua propria copia original obtida legalmente.
> Este repositorio nao possui afiliacao nem endosso da Capcom e nao inclui assets proprietarios do jogo.

Depois da instalacao:

1. Va para `build/application/`
2. Crie uma pasta chamada `resources`
3. Coloque `SF33RD.AFS` dentro dela

Caminho final:

```text
build/application/resources/SF33RD.AFS
```

## Situacao da validacao em hardware real

A compilacao e a execucao em hardware Linux real ainda estao pendentes. Os scripts de build foram revisados estaticamente, mas o fluxo completo do jogo precisa ser testado em uma maquina Linux nativa antes que esta plataforma seja considerada validada.

Os testes pendentes incluem inicializacao, selecao e cancelamento da ISO, extracao do `SF33RD.AFS`, escrita portatil, `error.log`, saves no MC1/MC2, replays, audio, input, comportamento da janela, encerramento e resolucao das bibliotecas compartilhadas.
