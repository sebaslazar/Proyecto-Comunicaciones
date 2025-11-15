@echo off
chcp 65001 >nul
cls
setlocal

echo ===================================================
echo  COMPILADOR AUTOMÁTICO - CLIENTE WINDOWS
echo  Carpeta: %cd%
echo  Fecha/hora: %date% %time%
echo ===================================================
echo.

rem ---------- comprueba fuente ----------
if not exist "cliente.c" (
  echo ERROR: El archivo cliente.c NO se ha encontrado en %cd%.
  echo Asegúrese de que el archivo esté en la misma carpeta.
  echo Contenido de la carpeta:
  dir /b
  echo.
  pause
  exit /b 1
)

rem ---------- comprobar gcc ----------
where gcc >nul 2>&1
if errorlevel 1 (
  echo.
  echo ERROR: gcc no encontrado en PATH.
  echo.
  echo Escribe en CMD: where gcc
  echo.
  echo Instalación recomendada:
  echo 1. Descargar MinGW-w64 desde: https://www.mingw-w64.org/
  echo 2. Agregar la carpeta bin de MinGW al PATH del sistema
  echo 3. Reiniciar la consola
  echo.
  pause
  exit /b 1
)

echo gcc encontrado:
where gcc
echo.
gcc --version
echo.
echo Limpiando logs antiguos...
del compile_log.txt 2>nul

rem ---------- compilar ----------
echo Compilando cliente.c...
gcc cliente.c -o cliente.exe -lws2_32 -O2 > compile_log.txt 2>&1

if errorlevel 1 (
  echo.
  echo ERROR: La compilación ha fallado. Mostrando compile_log.txt:
  echo ---------------------------------------------------------
  type compile_log.txt
  echo ---------------------------------------------------------
  echo.
  pause
  exit /b 1
)

echo.
echo ===============================================
echo    ¡COMPILACIÓN EXITOSA!
echo ===============================================
echo.
dir cliente.exe
for %%F in (cliente.exe) do echo Tamaño: %%~zF bytes
echo.

rem ---------- ejecutar inmediatamente ----------
echo ----------------------------------------------------------
echo  Ejecutando cliente.exe compilado
echo ----------------------------------------------------------
echo.
cliente.exe
set rc=%errorlevel%
echo.
echo ----------------------------------------------------------
echo cliente.exe terminó con código de salida: %rc%
echo ----------------------------------------------------------
echo.
pause
endlocal