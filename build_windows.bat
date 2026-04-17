@echo off
echo ========================================
echo Driving School Management - Build Script
echo ========================================
echo.
echo [INFO] Auto-fill uses Groq Vision API.
echo        Place your API key in groq_key.txt next to the .exe
echo        or set the GROQ_API_KEY environment variable.
echo        Free key: https://console.groq.com
echo.

REM Check if qmake is in PATH
where qmake >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: qmake not found in PATH
    echo Please run this script from Qt Command Prompt
    echo or add Qt bin directory to your PATH
    pause
    exit /b 1
)

echo Step 1: Cleaning old build files...
if exist Makefile del Makefile
if exist Makefile.Debug del Makefile.Debug
if exist Makefile.Release del Makefile.Release

echo Step 2: Generating Makefile...
qmake DrivingSchoolApp.pro
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: qmake failed
    pause
    exit /b 1
)

echo Step 3: Building application...
mingw32-make
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: Build failed
    echo If you're using MSVC, run 'nmake' instead of 'mingw32-make'
    pause
    exit /b 1
)

echo.
echo ========================================
echo Build completed successfully!
echo ========================================
echo.
echo To run the application:
echo 1. Navigate to the build folder
echo 2. Run DrivingSchoolApp.exe
echo.
echo Or press any key to try running it now...
pause

REM Try to find and run the executable
if exist release\DrivingSchoolApp.exe (
    echo Running release build...
    cd release
    DrivingSchoolApp.exe
) else if exist debug\DrivingSchoolApp.exe (
    echo Running debug build...
    cd debug
    DrivingSchoolApp.exe
) else (
    echo Could not find executable
    echo Please check the build output folders
    pause
)
