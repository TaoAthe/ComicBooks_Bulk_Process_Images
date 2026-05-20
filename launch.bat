@echo off
REM ComicVision Sorter - Simple Launch Script (Batch)
REM This script starts all components of the Comic Grading application

setlocal EnableExtensions EnableDelayedExpansion

set "ROOT=%~dp0"
set "BACKEND_DIR=%ROOT%build_root\Release"
set "BACKEND_EXE=%BACKEND_DIR%\ComicVisionSorterStage1.exe"
set "FRONTEND_DIR=%ROOT%frontend"
set "BORDER================================================="

echo(
echo %BORDER%
echo   ComicVision Sorter - Launcher
echo %BORDER%
echo(

REM Validate backend executable
if not exist "%BACKEND_EXE%" (
    echo [ERROR] Backend executable not found at:
    echo         %BACKEND_EXE%
    echo(
    echo Please build the project first using CMake.
    echo(
    pause
    exit /b 1
)

echo [OK] Backend executable found

REM Deploy Qt DLLs if Qt6Widgets.dll is missing
set "WINDEPLOYQT=%ROOT%comicvision-sorter-stage1\6.7.0\msvc2019_64\bin\windeployqt6.exe"
if not exist "%BACKEND_DIR%\Qt6Widgets.dll" (
    echo [INFO] Qt DLLs not found - running windeployqt6...
    if exist "%WINDEPLOYQT%" (
        "%WINDEPLOYQT%" --release "%BACKEND_EXE%" >nul 2>&1
        echo [OK] Qt DLLs deployed
    ) else (
        echo [WARNING] windeployqt6.exe not found at:
        echo           %WINDEPLOYQT%
        echo           If the app fails with a missing DLL error, run windeployqt6
        echo           manually from your Qt installation's bin folder.
    )
) else (
    echo [OK] Qt DLLs already present
)
echo(

REM Validate frontend workspace
if not exist "%FRONTEND_DIR%" (
    echo [ERROR] Frontend directory not found at:
    echo         %FRONTEND_DIR%
    echo(
    pause
    exit /b 1
)

echo [OK] Frontend directory found
echo(

REM Ensure frontend dependencies are present
if not exist "%FRONTEND_DIR%\node_modules" (
    echo [WARNING] Frontend dependencies not installed
    echo           Installing dependencies...
    echo(
    pushd "%FRONTEND_DIR%"
    call npm install
    if errorlevel 1 (
        echo [ERROR] Failed to install frontend dependencies
        popd
        pause
        exit /b 1
    )
    popd
    echo [OK] Frontend dependencies installed
    echo(
)

REM Optional: warn when Ollama is offline
curl -s http://localhost:11434/api/tags >nul 2>&1
if %errorlevel% equ 0 (
    echo [OK] Ollama is running
) else (
    echo [WARNING] Ollama is not running (optional for AI features)
    echo           To start Ollama, run: ollama serve
)

echo(
echo %BORDER%
echo   Starting Application Components
echo %BORDER%
echo(

REM Start backend
echo Starting Backend (Qt application)...
start "ComicVision Backend" /D "%BACKEND_DIR%" ComicVisionSorterStage1.exe
echo [OK] Backend started
echo(

REM Give backend time to bind to the port
timeout /t 2 /nobreak >nul

REM Start frontend dev server
echo Starting Frontend (Vite dev server)...
pushd "%FRONTEND_DIR%"
start "ComicVision Frontend" cmd /k "npm run dev"
popd
echo [OK] Frontend dev server starting
echo(

echo %BORDER%
echo   Application Launched Successfully
echo %BORDER%
echo(
echo Backend : Qt application is running
echo Frontend: http://localhost:5173
echo(
echo Both components are running in separate windows.
echo Close those windows to stop the application.
echo(
pause
