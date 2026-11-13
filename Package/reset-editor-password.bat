@echo off
rem A lost Editor password, or an account locked by too many tries (Docs/Design/78, section 3.6). Double-click it in the
rem world folder, on the world's computer, while the world runs (or in a terminal there: reset-editor-password.bat NAME).
rem It lists your world's Editor accounts, asks which one, and gives it a new one-time password and Editor code (shown
rem here, and written into manage\first-editor.txt too). The account's sessions end and its devices are forgotten; it
rem chooses a new password at its next sign-in. The same, typed:  docker compose exec world ratw-editor reset NAME
setlocal EnableExtensions DisableDelayedExpansion
cd /d "%~dp0"

docker compose version <nul >nul 2>&1
if errorlevel 1 (
    echo Docker Desktop isn't running, or isn't installed: start it, then double-click reset-editor-password.bat again.
    goto :done
)
docker compose exec -T world true <nul >nul 2>&1
if errorlevel 1 (
    echo Your world isn't running. Start it first ^(in this folder: docker compose up -d^), wait a minute, then
    echo double-click reset-editor-password.bat again.
    goto :done
)

set "NAME=%~1"
if defined NAME goto :chosen
echo Your world's Editor accounts:
echo.
docker compose exec -T world ratw-editor list <nul
echo.
set /p "NAME=Which account? Type its name (or just press Enter to stop): "
if not defined NAME (
    echo Nothing was changed.
    goto :done
)

:chosen
rem An account's name is small letters, digits, - and _ (3 to 32): anything else stops here.
set "ODD="
setlocal EnableDelayedExpansion
for /f "delims=abcdefghijklmnopqrstuvwxyz0123456789_-" %%A in ("!NAME!") do set "ODD=1"
endlocal & set "ODD=%ODD%"
if defined ODD (
    echo That isn't an Editor account's name ^(3 to 32 small letters, digits, - and _^). Nothing was changed.
    goto :done
)

echo.
docker compose exec -T world ratw-editor reset %NAME% --write-file <nul
if errorlevel 1 goto :done
echo.
echo Paste the Editor code into the game's EDITOR button ^(or use the name and one-time password^); you choose a new
echo password next. The code is also in manage\first-editor.txt. Keep it to yourself: it works once, for 72 hours.

:done
echo.
pause
endlocal
