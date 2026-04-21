# Build Guide: macOS

This guide explains how to build **3SX (Street Fighter III: 3rd Strike)** on macOS.

## 1. Verify Xcode Command Line Tools

Check whether the command line tools are installed:

```bash
xcode-select -p
```

If the command does not return a valid path, install them:

```bash
xcode-select --install
```

## 2. Prepare `third_party`

If the build complains about missing `.a` libraries, headers, or dependencies, check `third_party/` first.

Platform-specific build scripts are stored in `scripts/build/`.

For macOS:

```bash
bash scripts/build/build-deps-macos.sh
```

If you prefer the root shortcut, you can also run:

```bash
bash build-deps.sh
```

These scripts prepare FFmpeg, SDL3, GekkoNet, SDL3_net, libcdio, minizip-ng, and tf-psa-crypto in `third_party/`.

## 3. Configure and build

Configure the project:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
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
> `cmake --install` does not rebuild the app. It only copies and organizes artifacts already generated in `build/`.

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

# Guia de Compilacao: macOS

Este guia explica como compilar o **3SX (Street Fighter III: 3rd Strike)** no macOS.

## 1. Verifique as Xcode Command Line Tools

Confira se as ferramentas de linha de comando estao instaladas:

```bash
xcode-select -p
```

Se o comando nao retornar um caminho valido, instale as ferramentas:

```bash
xcode-select --install
```

## 2. Prepare `third_party`

Se o build reclamar de bibliotecas `.a`, headers ou dependencias faltando, verifique `third_party/` primeiro.

Os scripts especificos por plataforma ficam em `scripts/build/`.

Para macOS:

```bash
bash scripts/build/build-deps-macos.sh
```

Se preferir usar o atalho da raiz, voce tambem pode rodar:

```bash
bash build-deps.sh
```

Esses scripts preparam FFmpeg, SDL3, GekkoNet, SDL3_net, libcdio, minizip-ng e tf-psa-crypto em `third_party/`.

## 3. Configure e compile

Configure o projeto:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
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
> `cmake --install` nao recompila o app. Ele apenas copia e organiza os artefatos ja gerados em `build/`.

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
