# Build Guide

## Environment Setup

### Windows (recommended: MSYS2 + MinGW64)

This project is compiled via **MSYS2** with the **MinGW64** toolchain.

1. Install [MSYS2](https://www.msys2.org/).
   - Steps after #4 in the official instructions can be skipped.
2. Open the **MinGW64** shell (there should be a Start Menu shortcut called *MSYS2 MinGW 64-bit*).
3. Install the required packages:

    ```bash
    pacman -S --needed $(cat tools/requirements-windows.txt)
    ```

> ⚠️ **Important:** Always use the **MinGW64** shell, not the default MSYS2 or UCRT64. Using the wrong shell may cause linking errors with incompatible DLLs.

### Linux (Ubuntu)

```bash
sudo apt-get update
sudo apt-get install -y $(cat tools/requirements-ubuntu.txt)
```

### macOS

You should be able to build the project with just Xcode Command Line Tools.

1. Check if Command Line Tools are installed:

    ```bash
    xcode-select -p
    ```

2. Install if needed:

    ```bash
    xcode-select --install
    ```

---

## Building the Project

All commands below must be run from the repository root, inside the **MinGW64** shell (Windows) or terminal (Linux/macOS).

### 1. Build dependencies

```bash
sh build-deps.sh
```

> This step may take several minutes the first time. It compiles all third-party libraries (FFmpeg, SDL3, libcdio, etc.) locally into the `third_party/` folder.

### 2. Build the game

```bash
CC=clang cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel --config Release
cmake --install build --prefix build/application
```

The final output will be in `build/application/bin/`.

### 3. Output folder structure

After compiling and installing, the `build/application/bin/` folder will contain:

```
build/application/bin/
├── SF3.exe           ← game executable
├── *.dll             ← required libraries (SDL3, FFmpeg, etc.)
└── licenses/         ← third-party license notices
```

> **There is no "copy to another location" step.** Unlike the original project, **all game files (saves, config, keymap) are automatically created inside the executable's own folder**, in a `data/` subdirectory. The game is 100% portable.

### 4. (Optional) Create a distributable package

To generate a `.zip` file ready for distribution:

```bash
cd build && cpack -G ZIP
```

---

## Data File Locations

| File           | Location                                          |
|----------------|---------------------------------------------------|
| Save / progress| `<game folder>/data/`                             |
| Configuration  | `<game folder>/data/config`                       |
| Key mapping    | `<game folder>/data/keymap`                       |

> **Difference from the original project:** The upstream project stored these files at `C:\Users\<user>\AppData\Roaming\CrowdedStreet\3SX\` (Windows) or `~/.local/share/CrowdedStreet/3SX/` (Linux). In this fork, everything stays next to the executable.

---

## Troubleshooting

### Error: `cmake: command not found`
Make sure you are in the **MinGW64** shell from MSYS2 and that the `cmake` package was installed via `pacman`.

### Error: `ZLIB not found`
Install zlib through MSYS2:
```bash
pacman -S mingw-w64-x86_64-zlib
```

### Compile error with GCC (type mismatch)
This project is optimized for **Clang**. Use `CC=clang` when invoking cmake, as shown above.

### Game opens but closes immediately
Check that the game's `.iso` file is correctly referenced. See the [config documentation](config.md) for details on pointing to the game file.

---
---

<!--  🇧🇷 ──────────────────── PORTUGUÊS ────────────────────── 🇧🇷  -->

---

# Guia de Compilação

## Configuração do ambiente

### Windows (recomendado: MSYS2 + MinGW64)

Este projeto é compilado via **MSYS2** com o toolchain **MinGW64**.

1. Instale o [MSYS2](https://www.msys2.org/).
   - Os passos após o #4 das instruções oficiais podem ser ignorados.
2. Abra o shell **MinGW64** (deve haver um atalho no menu Iniciar chamado *MSYS2 MinGW 64-bit*).
3. Instale os pacotes necessários:

    ```bash
    pacman -S --needed $(cat tools/requirements-windows.txt)
    ```

> ⚠️ **Atenção:** Use sempre o shell **MinGW64**, não o MSYS2 padrão nem o UCRT64. O uso do shell errado pode causar erros de linking com DLLs incompatíveis.

### Linux (Ubuntu)

```bash
sudo apt-get update
sudo apt-get install -y $(cat tools/requirements-ubuntu.txt)
```

### macOS

Você deve conseguir compilar o projeto apenas com as Xcode Command Line Tools.

1. Verifique se as Command Line Tools estão instaladas:

    ```bash
    xcode-select -p
    ```

2. Instale se necessário:

    ```bash
    xcode-select --install
    ```

---

## Compilando o projeto

Todos os comandos abaixo devem ser executados na raiz do repositório, dentro do shell **MinGW64** (Windows) ou terminal (Linux/macOS).

### 1. Compilar dependências

```bash
sh build-deps.sh
```

> Este passo pode demorar vários minutos na primeira vez. Ele compila todas as bibliotecas de terceiro (FFmpeg, SDL3, libcdio, etc.) localmente dentro da pasta `third_party/`.

### 2. Compilar o jogo

```bash
CC=clang cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel --config Release
cmake --install build --prefix build/application
```

O resultado final estará em `build/application/bin/`.

### 3. Estrutura da pasta gerada

Após a compilação e instalação, a pasta `build/application/bin/` conterá:

```
build/application/bin/
├── SF3.exe           ← executável do jogo
├── *.dll             ← bibliotecas necessárias (SDL3, FFmpeg, etc.)
└── licenses/         ← avisos de licença de terceiros
```

> **Não há etapa de "copiar para outro lugar".** Diferente do projeto original, **todos os arquivos do jogo (saves, configurações, keymap) são criados automaticamente dentro da própria pasta do executável**, em uma subpasta `data/`. O jogo é 100% portátil.

### 4. (Opcional) Criar pacote distribuível

Para gerar um arquivo `.zip` pronto para distribuição:

```bash
cd build && cpack -G ZIP
```

---

## Localização dos arquivos de dados

| Arquivo            | Localização                                       |
|--------------------|---------------------------------------------------|
| Save / progresso   | `<pasta do jogo>/data/`                           |
| Configuração       | `<pasta do jogo>/data/config`                     |
| Mapeamento         | `<pasta do jogo>/data/keymap`                     |

> **Diferença em relação ao original:** O projeto upstream armazenava esses arquivos em `C:\Users\<usuário>\AppData\Roaming\CrowdedStreet\3SX\` (Windows) ou `~/.local/share/CrowdedStreet/3SX/` (Linux). Neste fork, tudo fica junto com o executável.

---

## Solução de problemas comuns

### Erro: `cmake: command not found`
Certifique-se de estar no shell **MinGW64** do MSYS2 e que o pacote `cmake` foi instalado via `pacman`.

### Erro: `ZLIB not found`
Instale o zlib pelo MSYS2:
```bash
pacman -S mingw-w64-x86_64-zlib
```

### Erro de compilação com GCC (incompatibilidade de tipo)
Este projeto é otimizado para **Clang**. Use `CC=clang` ao invocar o cmake, conforme mostrado acima.

### O jogo abre mas fecha imediatamente
Verifique se o arquivo `.iso` do jogo está corretamente referenciado. Consulte a [documentação de configuração](config.md) para detalhes sobre como apontar para o arquivo do jogo.
