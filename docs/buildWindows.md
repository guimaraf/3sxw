# Guia de Compilação: Windows

Neste guia, você encontrará o passo a passo para compilar o **3SX (Street Fighter III: 3rd Strike)** nativamente para o Windows. Toda a construção neste SO depende do ambiente MSYS2.

## Ambiente Suportado

Para garantir o sucesso na compilação, é altamente recomendável utilizar exatamente as seguintes ferramentas:

- **Windows 10 ou Windows 11**.
- **MSYS2** (obrigatório).
- Shell executado: **`MSYS2 MinGW 64-bit`** (não utilize UCRT64, nem o prompt do Visual Studio, muito menos o PowerShell padrão do Windows).
- Compilador **Clang** nativo do MSYS2.
- Gerador **CMake + Ninja**.

> [!WARNING]
> Usar o shell ou arquitetura (toolchain) incorreta frequentemente resulta em erros de "command not found", quebra de caminhos ou geração corrompida de DLLs e executáveis. Sempre confira se abriu o atalho certo no seu menu Iniciar: **"MSYS2 MinGW 64-bit"**.

---

## 1. Instalando as Ferramentas (Packages) Necessárias

Execute o MSYS2 (`MSYS2 MinGW 64-bit`) e instale todos os pacotes nativos para compilar usando a lista `tools/requirements-windows.txt`:

```bash
# Sincroniza e instala as dependências
pacman -Sy --needed $(cat tools/requirements-windows.txt)
```

Esses pacotes englobam o Git, o compilador Clang (`mingw-w64-x86_64-clang`), os construtores de projeto (CMake e Ninja), interpretadores de sintaxe (NASM) e bibliotecas brutas vitais como a Zlib.

---

## 2. Ponto de Atenção Crítico: A Pasta `third_party`

Um fator comum de quebras da compilação, **como o erro de o compilador não encontrar bibliotecas ou regras para arquivos `.a`**, ocorre se você limpar os dados do repositório inadvertidamente e apagar as bibliotecas terceiras localizadas na aba `third_party/`.

**NUNCA remova a compilação da pasta `third_party` se você não pretende gerá-la novamente.** Diversos sistemas (FFmpeg, GekkoNet, SDL3) rodam embutidos nessa localidade.

Para resolver dependências ausentes, rode a partir da raiz do repositório:

```bash
sh build-deps.sh
```

Esse script extrai e automatiza as rotinas de build para componentes paralelos, jogando-os de volta na pasta `third_party/` em segurança. Este processo pode demorar alguns minutos.

---

## 3. Configuração do Projeto e Compilação

Após atestar que o MSYS2 está carregado e validado e que os pacotes third-party foram populados, configure as regras de arquitetura:

```bash
# Configure o projeto na pasta "build", gerando com Ninja e Clang
CC=clang CXX=clang++ cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
```

Em seguida, dispare de fato o processo de build para a construção paralela (`-j 4` ou `--parallel` garante mais de 1 núcleo físico acelerando o link das lógicas):

```bash
cmake --build build --parallel
```
> [!NOTE] 
> O resultado no final deverá apontar `[100%] Linking C executable SF3.exe` (ou número similar de arquivos vinculados com êxito).

---

## 4. Finalização: Etapa de Instalação (Fundir os Binários)

Um erro muito comum no processo final é achar que compilar o projeto em `build` basta para rodá-lo. Na realidade, ferramentas como SDL3 e FFmpeg operam com DLLs acopladas ao sistema. Portanto você DEVE criar as cópias combinadas no pacote de Output através do CMake Install. 

Realize a instação (cópias finais):

```bash
cmake --install build --prefix build/application
```

Isso instruirá o CMake a coletar o `SF3.exe`, buscar todas as bibliotecas vinculadas durante a compilação (zlib1.dll, libstdc++-6.dll, SDL3.dll, etc.) e empacotá-las em definitivo para dentro do diretório `build/application/bin/`.

---

## 5. REQUISITO FINAL: Criação OBRIGATÓRIA da pasta resources e arquivo AFS

> [!IMPORTANT]
> Mesmo após uma compilação perfeita do `.exe` e DLLs, **o jogo não abrirá ou gerará erro silencioso se ele não encontrar os seus ativos de base, extraídos da versão orginal!**

Para isso:
1. Navegue para a pasta compilada onde se encontra o binário (exemplo: `build/application/bin/`).
2. Crie uma pasta raiz com o nome exato: **`resources`**.
3. Obtenha o arquivo maciço contido puramente na versão de console e copie-o para o final da rotina: `resources/SF33RD.AFS`.

Dessa maneira, o binário que criamos terá todos os recursos visuais de áudio (ADX) e arte carregados pela engrenagem original em tempo de execução e o jogo rodará tranquilamente no PC.