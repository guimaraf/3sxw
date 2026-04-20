# Guia de Compilação: macOS

Siga este procedimento para habilitar o processo de extração e link do binário portátil do **3SX (Street Fighter III: 3rd Strike)** via ambiente macOS e o clang empacotado da maçã. O benefício prático aqui é a facilidade das ferramentas C embutidas.

## 1. Verificando as "Command Line Tools" do Xcode

Você provavalmente não precisará emular ecossistemas paralelos ou pacotes externos, desde que o Xcode C/C++ já esteja acoplado por linha de comando em seu Apple.

Faça a verificação básica a fim de ter certeza que tem um construtor presente:

```bash
xcode-select -p
```
*Se uma rota de validação de developer application for retornada, significa que suas command tools já são aptas.*

Do contrário, **realize o download automático emulado pela própria via do sistema**:

```bash
xcode-select --install
```

Um prompt solicitará a confirmação no sistema. É uma rápida instalação.

---

## 2. Atenção Durante Operações Terceirizadas

Semelhante ao que é referenciado nas arquiteturas irmãs: jamais force uma compilação de cmake com a pasta raiz `third_party/` em falta ou corrompida se você tem limpado arquivos. Se o *terminal Shell* em algum ponto alertar que estão faltando livrarias ou regras isoladas para `.a`, é porque você esvaziou a sua sub-árvore local ou faltam updates nelas. 

Injete o processo automatizado do mantenedor do port para reconstruí-los a partir da raiz do Git:

```bash
sh build-deps.sh
```

---

## 3. Pipeline de Building macOS

Feito o processo preliminar sem nenhuma interrupção, inicie o roteamento do build sem adicionar explicitamente diretivas de clang fora da base - o sistema saberá qual alocação Mac-Native utilizar.

```bash
# Aloca o diretório temporário Release.
cmake -B build -DCMAKE_BUILD_TYPE=Release
```

Desencadeie o paralelismo das tasks para uma renderização muito mais veloz contendo todos os subgrupos da biblioteca `third_party`:

```bash
cmake --build build --parallel
```

---

## 4. O Passo de Empacotamento Binário Mac

No Mac, os binários rodam através não necessariamente de `.exe`, mas sim em torno de pastas modulares do tipo `.app`. As libs compartilhadas dinâmicas `.dylib` precisam ser atreladas aos caminhos corretos dentro dessas estruturas em árvore e contidas a dedo para que o Mac o interprete como um aplicativo sadio.

O comando Install do CMAke é essencial aqui e soluciona a conversão que aturdiria processos falhos de copias estáticas:

```bash
# Isso envia os links, recursos e executável final para a target designada. 
cmake --install build --prefix build/application
```

O App pronto deve estar contido em `/build/application`.

---

## 5. Adicionando a Arte do Jogo: Pasta `resources/`

> [!CAUTION]
> A janela ficará preta permenentemente, ou se recusará a subir o render se o arquivo principal de disco extraído do PlayStation 2 que guarda a totalidade da trilha sonora ADX e os bytescenes não estiver localizado como um asset dependente.

Se certifique inteiramente de:

1. Abrir os diretórios finais processados pelo passo (Etapa 4) de Instalação.
2. Criar fisicamente uma pasta chamada de **`resources`** anexa a execução do seu projeto no mesmo grau de hierarquia que processa o *root_path* da aplicação port.
3. Copiar e colar sem renomear o arquivo original com o nome **`SF33RD.AFS`**. Isso vai instanciar a inicialização da engine.