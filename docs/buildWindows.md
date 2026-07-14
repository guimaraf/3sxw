# Build Guide: Windows

This guide explains how to build **3SX (Street Fighter III: 3rd Strike)** on Windows.

Clone the repository with its submodules, or initialize them in an existing checkout:

```bash
git clone --recurse-submodules <REPOSITORY_URL>
git submodule update --init --recursive
```

## Supported environment

Recommended setup:
- Windows 10 or Windows 11
- MSYS2
- `MSYS2 MinGW 64-bit` shell
- Clang from MSYS2
- CMake + Ninja

> [!WARNING]
> Use the correct shell. This project is expected to build from **MSYS2 MinGW 64-bit**.
>
> The command `CC=clang CXX=clang++ cmake ...` is Unix/MSYS2 shell syntax. It does **not** work in regular PowerShell.

## 1. Install required packages

Inside `MSYS2 MinGW 64-bit`, run:

```bash
pacman -Syu
pacman -S --needed $(cat tools/requirements-windows.txt)
```

Restart the MSYS2 shell if the system update requests it, then run the second command. This installs Git, Clang, CMake, Ninja, NASM, Zlib, and the other required packages.

If the repository is on another drive, move into the project directory manually before building.

Example for this repository:

```bash
cd /f/GitRevised/3sxw
```

In MSYS2, a Windows path like `F:\GitRevised\3sxw` becomes `/f/GitRevised/3sxw`.

## 2. Recommended automated build

From Command Prompt or PowerShell in the project root, run this for the first build or after dependency changes:

```bat
build.bat deps
```

The script invokes the MSYS2 toolchain, configures a Release build, compiles it, and installs the complete portable application. For subsequent Release builds, use:

```bat
build.bat
```

For a Debug build:

```bat
build.bat Debug
```

MSYS2 is expected at `C:\msys64` by default. If it is installed elsewhere, set `MSYS2_ROOT` first:

```bat
set MSYS2_ROOT=D:\path\to\msys64
build.bat deps
```

The remaining sections describe the equivalent manual workflow.

## 3. Prepare `third_party` manually

If the build complains about missing `.a` libraries, headers, or dependencies, check `third_party/` first.

Platform-specific build scripts are stored in `scripts/build/`.

For Windows:

```bash
bash scripts/build/build-deps-windows.sh
```

If you prefer the root shortcut, you can also run:

```bash
bash build-deps.sh
```

These scripts prepare FFmpeg, SDL3, libcdio, minizip-ng, and tf-psa-crypto in `third_party/`.

## 4. Configure and build manually

Configure the project:

```bash
CC=clang CXX=clang++ cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

> [!NOTE]
> If you change `CMakeLists.txt` or any install rule, run the configure command again before using `cmake --build` or `cmake --install`.

Build the project:

```bash
cmake --build build --parallel
```

> [!NOTE]
> If nothing changed since the last successful build, Ninja may report `no work to do.`. That is expected.

> [!WARNING]
> If CMake configuration fails halfway through, `build/` can become inconsistent. If Ninja later complains about files such as `CMakeFiles/rules.ninja`, clean at least:
>
> ```text
> build/CMakeCache.txt
> build/CMakeFiles/
> ```
>
> If needed, remove the entire `build/` directory and configure again.

## 5. Install the final output

Build output alone is not enough. Install assembles the executable, runtime DLLs, and support files into the final folder:

```bash
cmake --install build --prefix build/application
```

> [!NOTE]
> `cmake --install` does not rebuild the project. It only copies and organizes artifacts already produced in `build/`.

## 6. Final layout and game resource

> [!IMPORTANT]
> The game will not run correctly without the original assets.
> You must use your own legally obtained original copy of the game.
> This repository is not affiliated with or endorsed by Capcom and does not include proprietary game assets.

The installed application contains:

```text
build/application/bin/SF3.exe
build/application/bin/sf3config.exe
build/application/bin/data/img/bezel.png
```

The configurator's intermediate executable is generated at `appConfig/build/sf3config.exe`. The install step copies it beside `SF3.exe` together with the runtime DLLs and bundled bezel.

Create `build/application/bin/resources/` and place your legally obtained resource at:

```text
build/application/bin/resources/SF33RD.AFS
```

The final `bin/` directory must remain writable because saves, replays, configuration, screenshots, and critical logs are stored with the portable game.

---

# Guia de Compilacao: Windows

Este guia explica como compilar o **3SX (Street Fighter III: 3rd Strike)** no Windows.

Clone o repositorio com seus submodulos ou inicialize-os em um checkout existente:

```bash
git clone --recurse-submodules <URL_DO_REPOSITORIO>
git submodule update --init --recursive
```

## Ambiente suportado

Configuracao recomendada:
- Windows 10 ou Windows 11
- MSYS2
- shell `MSYS2 MinGW 64-bit`
- Clang do MSYS2
- CMake + Ninja

> [!WARNING]
> Use o shell correto. Este projeto deve ser compilado a partir do **MSYS2 MinGW 64-bit**.
>
> O comando `CC=clang CXX=clang++ cmake ...` e sintaxe de shell Unix/MSYS2. Ele **nao** funciona no PowerShell comum.

## 1. Instale os pacotes necessarios

Dentro do `MSYS2 MinGW 64-bit`, rode:

```bash
pacman -Syu
pacman -S --needed $(cat tools/requirements-windows.txt)
```

Reinicie o shell do MSYS2 se a atualizacao do sistema solicitar e depois execute o segundo comando. Isso instala Git, Clang, CMake, Ninja, NASM, Zlib e os outros pacotes necessarios.

Se o repositorio estiver em outro disco, entre manualmente na pasta do projeto antes de compilar.

Exemplo para este repositorio:

```bash
cd /f/GitRevised/3sxw
```

No MSYS2, um caminho Windows como `F:\GitRevised\3sxw` vira `/f/GitRevised/3sxw`.

## 2. Compilacao automatizada recomendada

No Prompt de Comando ou PowerShell aberto na raiz do projeto, use este comando na primeira compilacao ou depois de alterar dependencias:

```bat
build.bat deps
```

O script chama o toolchain do MSYS2, configura uma build Release, compila e instala a aplicacao portatil completa. Nas proximas builds Release, use:

```bat
build.bat
```

Para uma build Debug:

```bat
build.bat Debug
```

Por padrao, o MSYS2 e procurado em `C:\msys64`. Se estiver instalado em outro local, defina `MSYS2_ROOT` antes:

```bat
set MSYS2_ROOT=D:\caminho\msys64
build.bat deps
```

As proximas secoes descrevem o fluxo manual equivalente.

## 3. Prepare `third_party` manualmente

Se o build reclamar de bibliotecas `.a`, headers ou dependencias faltando, verifique `third_party/` primeiro.

Os scripts especificos por plataforma ficam em `scripts/build/`.

Para Windows:

```bash
bash scripts/build/build-deps-windows.sh
```

Se preferir usar o atalho da raiz, voce tambem pode rodar:

```bash
bash build-deps.sh
```

Esses scripts preparam FFmpeg, SDL3, libcdio, minizip-ng e tf-psa-crypto em `third_party/`.

## 4. Configure e compile manualmente

Configure o projeto:

```bash
CC=clang CXX=clang++ cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

> [!NOTE]
> Se voce alterar `CMakeLists.txt` ou qualquer regra de install, rode novamente o comando de configuracao antes de usar `cmake --build` ou `cmake --install`.

Compile o projeto:

```bash
cmake --build build --parallel
```

> [!NOTE]
> Se nada mudou desde o ultimo build bem-sucedido, o Ninja pode responder `no work to do.`. Isso e esperado.

> [!WARNING]
> Se a configuracao do CMake falhar no meio do processo, `build/` pode ficar inconsistente. Se depois o Ninja reclamar de arquivos como `CMakeFiles/rules.ninja`, limpe pelo menos:
>
> ```text
> build/CMakeCache.txt
> build/CMakeFiles/
> ```
>
> Se necessario, apague todo o diretorio `build/` e configure de novo.

## 5. Instale a saida final

O resultado do build sozinho nao basta. A instalacao monta o executavel, as DLLs de runtime e os arquivos de suporte na pasta final:

```bash
cmake --install build --prefix build/application
```

> [!NOTE]
> `cmake --install` nao recompila o projeto. Ele apenas copia e organiza os artefatos ja gerados em `build/`.

## 6. Estrutura final e recurso do jogo

> [!IMPORTANT]
> O jogo nao vai funcionar corretamente sem os assets originais.
> Voce deve usar sua propria copia original obtida legalmente.
> Este repositorio nao possui afiliacao nem endosso da Capcom e nao inclui assets proprietarios do jogo.

A aplicacao instalada contem:

```text
build/application/bin/SF3.exe
build/application/bin/sf3config.exe
build/application/bin/data/img/bezel.png
```

O executavel intermediario do configurador e gerado em `appConfig/build/sf3config.exe`. O passo de instalacao o copia para o mesmo local de `SF3.exe`, junto das DLLs de runtime e da moldura fornecida pelo projeto.

Crie `build/application/bin/resources/` e coloque o recurso obtido legalmente em:

```text
build/application/bin/resources/SF33RD.AFS
```

A pasta final `bin/` precisa permitir escrita porque saves, replays, configuracoes, capturas de tela e logs criticos permanecem junto do jogo portatil.
