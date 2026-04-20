# Guia de Compilação: Linux

Este documento traz as instruções para compilar o **3SX (Street Fighter III: 3rd Strike)** nativamente em distribuições baseadas em Linux (Debian/Ubuntu). A grande vantagem deste sistema é sua estabilidade e acesso fácil aos pacotes pelo APT Unix.

## 1. Instalação de Pacotes Essenciais

Antes de qualquer compilação do jogo principal ou de bibliotecas pesadas de base, utilize os empacotadores da sua distribuição para obter todas as ferramentas principais:

```bash
sudo apt-get update
sudo apt-get install -y $(cat tools/requirements-ubuntu.txt)
```

Isso proverá o Clang e todas as bibliotecas elementares para manuseio.

---

## 2. Ponto de Atenção: Bibliotecas em `third_party/`

Se o compilador acusar falta de DLLs, arquivos de biblioteca `.a` ou `missing header files` durante a construção final (Make / Ninja build pipeline), é bastante comum que haja arquivos deletados ou dependências não-compiladas contidas nativamente no diretório `third_party/` localizado na raiz deste repositório.

**Você precisa ter certeza que elas existem.** Sempre construa essas extensões utilizando o comando fornecido:

```bash
# Você deve estar na raiz do repositório
sh build-deps.sh
```

Esse comando preparará SDL3, FFmpeg, dependências GekkoNet e drivers CDIO, injetando-os automaticamente na estrutura do pacote final isolando dependências de sistema com problema.

---

## 3. Configuração e Compilação Principal

Em seguida, basta orientar a compilação utilizando o Clang já previamente adquirido durante o carregamento das ferramentas nas configurações Release:

```bash
# Especifica clang e gera caminhos do build
CC=clang CXX=clang++ cmake -B build -DCMAKE_BUILD_TYPE=Release
```

Execute o processamento com o paralelismo habilitado para acelerar o tempo de link entre objetos C:

```bash
cmake --build build --parallel
```

---

## 4. Etapa de Instalação (Reagrupamento)

Se você compilar o binário, o jogo não vai subir propriamente do arquivo `.so` contido dentro de `/build`. Para atrelar as bibliotecas geradas via `chmod` (runtime libraries e shared objects), dispare o construtor final em formato de install:

```bash
cmake --install build --prefix build/application
```

Isso irá despachar o binário pronto para a pasta de aplicação contendo seu `.so` e diretivas.

---

## 5. Regra Vital: Importação de Ativos (Assets do Jogo)

> [!WARNING]
> Independentemente da perfeição da sua configuração ou se a tela abriu antes, **sem os assets visuais originais você terá uma tela vazia em terminal ou fechamento fatal (crash) silenciado!**

Para sanar este problema e dar vida ao seu compilado final de Unix:
1. Vá até o local de processamento base (`build/application/`).
2. Crie manualmente a pasta de injeção de leitura: **`resources`**.
3. Adicione lá internamente o maior cofre de áudios (ADX) e visuais da engine, diretamente do disco referencial original: **`SF33RD.AFS`**.

Ficando assim o caminho: `build/application/resources/SF33RD.AFS`. Quando disparar seu script de gameplay do binário no Ubuntu, ele reconhecerá as rotas em sua totalidade de primeira.