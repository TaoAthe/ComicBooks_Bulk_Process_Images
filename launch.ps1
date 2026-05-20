#!/usr/bin/env pwsh
# ComicVision Sorter - Launch Script
# This script starts all components of the Comic Grading application

param(
    [switch]$BackendOnly,
    [switch]$FrontendOnly,
    [switch]$Help
)

$ErrorActionPreference = "Stop"

# Color output functions
function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Info { Write-Host $args -ForegroundColor Cyan }
function Write-Warning { Write-Host $args -ForegroundColor Yellow }
function Write-Error { Write-Host $args -ForegroundColor Red }

function Show-Help {
    Write-Host @"
ComicVision Sorter - Launch Script
===================================

Usage: .\launch.ps1 [options]

Options:
    -BackendOnly    Launch only the Qt C++ backend application
    -FrontendOnly   Launch only the React frontend dev server
    -Help           Show this help message

Examples:
    .\launch.ps1                # Launch both frontend and backend
    .\launch.ps1 -BackendOnly   # Launch only backend
    .\launch.ps1 -FrontendOnly  # Launch only frontend

Prerequisites:
    - Qt 6 installed and built (backend exe should exist)
    - Node.js installed (for frontend)
    - Ollama running (optional, for AI features)

"@
    exit 0
}

if ($Help) {
    Show-Help
}

# Define paths
$ScriptRoot = $PSScriptRoot
$BackendDir = Join-Path $ScriptRoot "build_root\Release"
$BackendExe = Join-Path $BackendDir "ComicVisionSorterStage1.exe"
$FrontendDir = Join-Path $ScriptRoot "frontend"

# Check prerequisites
function Test-Prerequisites {
    Write-Info "`n=== Checking Prerequisites ==="
    
    # Check if backend executable exists
    if (-not $FrontendOnly) {
        if (Test-Path $BackendExe) {
            Write-Success "✓ Backend executable found: $BackendExe"
        } else {
            Write-Error "✗ Backend executable not found at: $BackendExe"
            Write-Warning "  Please build the project first using CMake"
            return $false
        }
    }
    
    # Check if frontend directory exists
    if (-not $BackendOnly) {
        if (Test-Path $FrontendDir) {
            Write-Success "✓ Frontend directory found: $FrontendDir"
        } else {
            Write-Error "✗ Frontend directory not found at: $FrontendDir"
            return $false
        }
        
        # Check if node_modules exists
        $NodeModules = Join-Path $FrontendDir "node_modules"
        if (-not (Test-Path $NodeModules)) {
            Write-Warning "✗ Frontend dependencies not installed"
            Write-Info "  Installing frontend dependencies..."
            Push-Location $FrontendDir
            try {
                npm install
                Write-Success "✓ Frontend dependencies installed"
            } catch {
                Write-Error "✗ Failed to install frontend dependencies"
                Pop-Location
                return $false
            }
            Pop-Location
        } else {
            Write-Success "✓ Frontend dependencies installed"
        }
    }
    
    # Check if Ollama is running (optional)
    try {
        $response = Invoke-WebRequest -Uri "http://localhost:11434/api/tags" -Method Get -TimeoutSec 2 -ErrorAction SilentlyContinue
        Write-Success "✓ Ollama is running"
    } catch {
        Write-Warning "⚠ Ollama is not running (optional for AI features)"
        Write-Info "  To start Ollama, run: ollama serve"
    }
    
    return $true
}

function Start-Backend {
    Write-Info "`n=== Starting Backend (Qt Application) ==="
    Write-Info "Launching: $BackendExe"
    
    # Start the backend process
    $process = Start-Process -FilePath $BackendExe -WorkingDirectory (Split-Path $BackendExe) -PassThru
    
    Write-Success "✓ Backend started (PID: $($process.Id))"
    Write-Info "  Backend is now running..."
    
    return $process
}

function Start-Frontend {
    Write-Info "`n=== Starting Frontend (Vite Dev Server) ==="
    Write-Info "Working directory: $FrontendDir"
    
    Push-Location $FrontendDir
    
    # Start the frontend dev server in a new window
    $process = Start-Process powershell -ArgumentList "-NoExit", "-Command", "npm run dev" -PassThru
    
    Pop-Location
    
    Write-Success "✓ Frontend dev server starting (PID: $($process.Id))"
    Write-Info "  Frontend will be available at: http://localhost:5173"
    Write-Info "  (Vite may take a few seconds to start)"
    
    return $process
}

# Main execution
try {
    Write-Host @"

╔═══════════════════════════════════════╗
║   ComicVision Sorter - Launcher       ║
╚═══════════════════════════════════════╝

"@

    # Check prerequisites
    if (-not (Test-Prerequisites)) {
        Write-Error "`nPrerequisite check failed. Please resolve the issues above."
        exit 1
    }
    
    $processes = @()
    
    # Launch components based on flags
    if ($BackendOnly) {
        $backendProcess = Start-Backend
        $processes += $backendProcess
    }
    elseif ($FrontendOnly) {
        $frontendProcess = Start-Frontend
        $processes += $frontendProcess
    }
    else {
        # Launch both
        $backendProcess = Start-Backend
        Start-Sleep -Seconds 2  # Give backend time to start
        
        $frontendProcess = Start-Frontend
        
        $processes += $backendProcess
        $processes += $frontendProcess
    }
    
    Write-Host @"

╔═══════════════════════════════════════╗
║   Application Launched Successfully   ║
╚═══════════════════════════════════════╝

"@

    if (-not $FrontendOnly) {
        Write-Info "Backend:  Qt Application is running"
    }
    if (-not $BackendOnly) {
        Write-Info "Frontend: http://localhost:5173"
    }
    
    Write-Host ""
    Write-Warning "Press Ctrl+C to stop monitoring (processes will continue running)"
    Write-Info "To stop all processes, close their windows or use Task Manager"
    Write-Host ""
    
    # Keep script alive to monitor processes
    while ($true) {
        Start-Sleep -Seconds 5
        
        # Check if processes are still running
        foreach ($proc in $processes) {
            if ($proc.HasExited) {
                Write-Warning "Process $($proc.Id) has exited"
            }
        }
    }
    
} catch {
    Write-Error "An error occurred: $_"
    exit 1
}
