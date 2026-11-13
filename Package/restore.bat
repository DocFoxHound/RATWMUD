@echo off
rem Puts your world back to one of its backups (Docs/Design/78, section 12.2). Double-click it in the world folder,
rem or in a terminal there:  restore.bat [NAME]   (My world's "Copy restore steps" gives the line).
rem It says what you would lose and asks before anything changes. Then it stops the world (it saves first), makes a
rem safety copy of the world as it is now, puts the backup back (keeping today's Editor accounts, unless you choose the
rem backup's), ends every Editor session and starts the world again. The backup service's own "restore" refuses a
rem running world, a backup from another world, a damaged one and one made by a newer package. (restore.sh: the same.)
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%~dp0"

docker compose version <nul >nul 2>&1
if errorlevel 1 (
    echo Docker Desktop isn't running, or isn't installed: start it, then double-click restore.bat again.
    goto :done
)
if not exist compose.yaml (
    echo Run restore.bat from your world's folder ^(the one with compose.yaml^).
    goto :done
)

echo Stopping the backup service while you choose ^(it starts again at the end^)...
docker compose stop backup <nul >nul 2>&1

set "NAME=%~1"
if defined NAME goto :chosen
docker compose run --rm -T backup list <nul
if errorlevel 1 goto :again
set "NUMBER="
set /p "NUMBER=Which backup? Type its number (or just press Enter to stop): "
if not defined NUMBER (
    echo Nothing was changed.
    goto :again
)
rem Digits only: anything else (which cmd could read as a command) stops here.
set "ODD="
setlocal EnableDelayedExpansion
for /f "delims=0123456789" %%A in ("!NUMBER!") do set "ODD=1"
endlocal & set "ODD=%ODD%"
if defined ODD (
    echo That isn't a number. Nothing was changed.
    goto :again
)
set "NAME="
for /f "usebackq delims=" %%L in (`docker compose run --rm -T backup name %NUMBER% ^<nul`) do set "NAME=%%L"
if not defined NAME (
    echo Nothing was changed.
    goto :again
)

:chosen
rem A backup's name is letters, digits and dashes (ratw-20261112T140300Z-daily): anything else stops here.
set "ODD="
setlocal EnableDelayedExpansion
for /f "delims=0123456789abcdefghijklmnopqrstuvwxyzTZ-" %%A in ("!NAME!") do set "ODD=1"
endlocal & set "ODD=%ODD%"
if defined ODD (
    echo That isn't a backup's name. Nothing was changed.
    goto :again
)
docker compose run --rm -T backup show %NAME% <nul
if errorlevel 1 goto :again
set "SURE="
set /p "SURE=Type RESTORE to go on (anything else stops): "
rem (Compared with delayed expansion, so nothing typed is ever read as part of a command.)
set "GO="
setlocal EnableDelayedExpansion
if "!SURE!"=="RESTORE" set "GO=1"
endlocal & set "GO=%GO%"
if not defined GO (
    echo Nothing was changed.
    goto :again
)

echo.
echo Keep your Editor accounts as they are now ^(recommended^), or take the ones in the backup?
echo Keeping them means a co-builder you removed, an old password or a forgotten device doesn't come back.
set "WHICH="
set /p "WHICH=Type K to keep them (or just press Enter), or B to take the backup's: "
set "ACCOUNTS=--keep-accounts"
setlocal EnableDelayedExpansion
set "TAKE="
if defined WHICH if /i "!WHICH:~0,1!"=="B" set "TAKE=1"
endlocal & if "%TAKE%"=="1" set "ACCOUNTS=--take-accounts"

echo.
echo Stopping your world ^(it saves first: this takes up to a minute and a half^)...
docker compose stop world <nul
if errorlevel 1 (
    echo Your world didn't stop. Nothing was changed.
    goto :again
)
docker compose run --rm -T backup restore %NAME% %ACCOUNTS% <nul
set "CODE=%ERRORLEVEL%"
echo Starting your world again...
docker compose up -d <nul >nul 2>&1
if errorlevel 1 echo It didn't start: type docker compose up -d in this folder.
if "%CODE%"=="0" (
    echo Your world is back. Sign in to its Editor again ^(every Editor session ended^).
) else (
    echo The restore didn't finish ^(see above^).
)
goto :done

:again
echo Starting your world again...
docker compose up -d <nul >nul 2>&1
if errorlevel 1 echo It didn't start: type docker compose up -d in this folder.

:done
echo.
pause
endlocal
