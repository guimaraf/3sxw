@echo off
setlocal EnableExtensions

set "BUILD_TYPE=Release"
set "BUILD_DEPS=0"

:parse_arguments
if "%~1"=="" goto arguments_done
if /I "%~1"=="Release" (
    set "BUILD_TYPE=Release"
    shift
    goto parse_arguments
)
if /I "%~1"=="Debug" (
    set "BUILD_TYPE=Debug"
    shift
    goto parse_arguments
)
if /I "%~1"=="deps" (
    set "BUILD_DEPS=1"
    shift
    goto parse_arguments
)

echo Argumento desconhecido: %~1
echo Uso: build.bat [Release^|Debug] [deps]
exit /b 2

:arguments_done
if not defined MSYS2_ROOT set "MSYS2_ROOT=C:\msys64"
set "MSYS2_SHELL=%MSYS2_ROOT%\msys2_shell.cmd"
set "MSYS2_CMAKE=%MSYS2_ROOT:\=/%"

if not exist "%MSYS2_SHELL%" (
    echo ERRO: MSYS2 nao encontrado em "%MSYS2_ROOT%".
    echo Instale o MSYS2 nesse caminho ou defina MSYS2_ROOT antes de executar.
    exit /b 1
)

pushd "%~dp0"
if errorlevel 1 (
    echo ERRO: nao foi possivel acessar a pasta do projeto.
    exit /b 1
)

if "%BUILD_DEPS%"=="1" (
    echo Preparando dependencias de terceiros...
    call "%MSYS2_SHELL%" -defterm -no-start -here -mingw64 -c "bash ./scripts/build/build-deps-windows.sh"
    if errorlevel 1 goto build_failed
)

if not exist "third_party\sdl3\build\lib\libSDL3.dll.a" (
    echo ERRO: dependencias de terceiros nao encontradas.
    echo Execute primeiro: build.bat %BUILD_TYPE% deps
    goto build_failed
)

echo Configurando 3SX em modo %BUILD_TYPE%...
call "%MSYS2_SHELL%" -defterm -no-start -here -mingw64 -c "CC=clang CXX=clang++ cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DMSYS2_LOCATION=%MSYS2_CMAKE%"
if errorlevel 1 goto build_failed

echo Compilando 3SX...
call "%MSYS2_SHELL%" -defterm -no-start -here -mingw64 -c "cmake --build build --parallel"
if errorlevel 1 goto build_failed

echo Montando a pasta portatil...
call "%MSYS2_SHELL%" -defterm -no-start -here -mingw64 -c "cmake --install build --prefix build/application"
if errorlevel 1 goto build_failed

echo.
echo Build concluido com sucesso.
echo Saida: %CD%\build\application\bin
popd
exit /b 0

:build_failed
set "BUILD_RESULT=%ERRORLEVEL%"
if "%BUILD_RESULT%"=="0" set "BUILD_RESULT=1"
echo.
echo ERRO: a compilacao foi interrompida. Codigo %BUILD_RESULT%.
popd
exit /b %BUILD_RESULT%

pause