# Build Guide: Windows

This guide explains how to build **3SX (Street Fighter III: 3rd Strike)** on Windows.

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
pacman -Sy --needed $(cat tools/requirements-windows.txt)
```

This installs Git, Clang, CMake, Ninja, NASM, Zlib, and other required packages.

If the repository is on another drive, move into the project directory manually before building.

Example for this repository:

```bash
cd /f/GitRevised/3sxw
```

In MSYS2, a Windows path like `F:\GitRevised\3sxw` becomes `/f/GitRevised/3sxw`.

## 2. Prepare `third_party`

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

These scripts prepare FFmpeg, SDL3, GekkoNet, SDL3_net, libcdio, minizip-ng, and tf-psa-crypto in `third_party/`.

## 3. Configure and build

Configure the project:

```bash
CC=clang CXX=clang++ cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
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

## 4. Install the final output

Build output alone is not enough. Install assembles the executable, runtime DLLs, and support files into the final folder:

```bash
cmake --install build --prefix build/application
```

> [!NOTE]
> `cmake --install` does not rebuild the project. It only copies and organizes artifacts already produced in `build/`.

## 5. Add game assets

> [!IMPORTANT]
> The game will not run correctly without the original assets.

After installation:

1. Go to `build/application/`
2. Create a folder named `resources`
3. Place `SF33RD.AFS` inside it

Final path:

```text
build/application/resources/SF33RD.AFS
```

---

# Guia de Compilacao: Windows

Este guia explica como compilar o **3SX (Street Fighter III: 3rd Strike)** no Windows.

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
pacman -Sy --needed $(cat tools/requirements-windows.txt)
```

Isso instala Git, Clang, CMake, Ninja, NASM, Zlib e outros pacotes necessarios.

Se o repositorio estiver em outro disco, entre manualmente na pasta do projeto antes de compilar.

Exemplo para este repositorio:

```bash
cd /f/GitRevised/3sxw
```

No MSYS2, um caminho Windows como `F:\GitRevised\3sxw` vira `/f/GitRevised/3sxw`.

## 2. Prepare `third_party`

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

Esses scripts preparam FFmpeg, SDL3, GekkoNet, SDL3_net, libcdio, minizip-ng e tf-psa-crypto em `third_party/`.

## 3. Configure e compile

Configure o projeto:

```bash
CC=clang CXX=clang++ cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
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

## 4. Instale a saida final

O resultado do build sozinho nao basta. A instalacao monta o executavel, as DLLs de runtime e os arquivos de suporte na pasta final:

```bash
cmake --install build --prefix build/application
```

> [!NOTE]
> `cmake --install` nao recompila o projeto. Ele apenas copia e organiza os artefatos ja gerados em `build/`.

## 5. Adicione os assets do jogo

> [!IMPORTANT]
> O jogo nao vai funcionar corretamente sem os assets originais.

Depois da instalacao:

1. Va para `build/application/`
2. Crie uma pasta chamada `resources`
3. Coloque `SF33RD.AFS` dentro dela

Caminho final:

```text
build/application/resources/SF33RD.AFS
```
