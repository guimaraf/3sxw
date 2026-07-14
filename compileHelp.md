# Guia rapido de compilacao

O projeto usa CMake e mantem suas dependencias compiladas em `third_party/`. Clone o repositorio incluindo os submodulos:

```bash
git clone --recurse-submodules <URL_DO_REPOSITORIO>
```

Os assets originais do jogo nao fazem parte do repositorio. Use somente arquivos obtidos legalmente.

## Windows

Requisitos:

- Windows 10 ou 11.
- MSYS2 instalado, por padrao, em `C:\msys64`.
- Pacotes listados em `tools/requirements-windows.txt`.

No terminal **MSYS2 MinGW 64-bit**, instale os pacotes uma vez:

```bash
pacman -Syu
pacman -S --needed $(cat tools/requirements-windows.txt)
```

Depois, no Prompt de Comando ou PowerShell aberto na raiz do projeto:

```bat
build.bat deps
```

Esse comando prepara as dependencias, compila em Release e instala a pasta portatil. Nos builds seguintes, use apenas:

```bat
build.bat
```

Para compilar em Debug:

```bat
build.bat Debug
```

Se o MSYS2 estiver em outro local, defina antes:

```bat
set MSYS2_ROOT=D:\caminho\msys64
build.bat deps
```

Saida final:

```text
build/application/bin/SF3.exe
build/application/bin/sf3config.exe
```

O executavel intermediario do configurador e gerado em `appConfig/build/sf3config.exe` e copiado automaticamente para a aplicacao pelo passo de instalacao.

A moldura incluida no projeto e instalada automaticamente em:

```text
build/application/bin/data/img/bezel.png
```

Coloque o recurso legalmente obtido em:

```text
build/application/bin/resources/SF33RD.AFS
```

## Linux

Em Debian ou Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y $(cat tools/requirements-ubuntu.txt)
bash scripts/build/build-deps-linux.sh
CC=clang CXX=clang++ cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build --prefix build/application
```

Saida final:

```text
build/application/bin/SF3
build/application/bin/sf3config
```

O binario intermediario do configurador e gerado em `appConfig/build/`.

A moldura incluida no projeto e instalada automaticamente em:

```text
build/application/bin/data/img/bezel.png
```

As bibliotecas ficam em `build/application/lib/`. Coloque o recurso em:

```text
build/application/bin/resources/SF33RD.AFS
```

## macOS

Instale ou confirme as Xcode Command Line Tools:

```bash
xcode-select -p || xcode-select --install
```

Depois execute:

```bash
bash scripts/build/build-deps-macos.sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build --prefix build/application
```

Saida final:

```text
build/application/3SX.app
build/application/sf3config.app
```

O bundle intermediario do configurador e gerado em `appConfig/build/`.

A moldura incluida no projeto e instalada automaticamente em:

```text
build/application/3SX.app/Contents/MacOS/data/img/bezel.png
```

No macOS, a pasta portatil fica ao lado do bundle:

```text
build/application/resources/SF33RD.AFS
```

## Observacoes

- Execute novamente a configuracao do CMake depois de alterar `CMakeLists.txt`.
- Se a configuracao anterior estiver inconsistente, remova manualmente `build/CMakeCache.txt` e `build/CMakeFiles/` ou recrie a pasta `build/`.
- O passo `cmake --install` e necessario para copiar as DLLs ou bibliotecas e montar a estrutura portatil final.
- A pasta final precisa permitir escrita, pois saves, replays, configuracoes e logs criticos permanecem junto do jogo.
- Guias mais detalhados estao em `docs/buildWindows.md`, `docs/buildLinux.md` e `docs/buildMac.md`.
