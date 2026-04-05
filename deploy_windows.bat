@echo off
echo ========================================
echo Windows Deployment Script
echo ========================================
echo.
echo This script will:
echo 1. Copy Qt dependencies to your executable
echo 2. Create a portable distribution folder
echo.
pause

REM Check if windeployqt is available
where windeployqt >nul 2>&1
if %ERRORLEVEL% NEQ 0 (
    echo ERROR: windeployqt not found in PATH
    echo Please run this script from Qt Command Prompt
    pause
    exit /b 1
)

REM Check for release executable
if not exist release\DrivingSchoolApp.exe (
    echo ERROR: Release executable not found
    echo Please build the project in Release mode first
    echo.
    echo In Qt Creator: Select 'Release' from build config dropdown and rebuild
    pause
    exit /b 1
)

echo Creating deployment folder...
if exist deploy rmdir /s /q deploy
mkdir deploy

echo Copying executable...
copy release\DrivingSchoolApp.exe deploy\

echo Deploying Qt dependencies...
cd deploy
windeployqt DrivingSchoolApp.exe
cd ..

echo.
echo ========================================
echo Deployment completed successfully!
echo ========================================
echo.
echo Your portable application is in the 'deploy' folder
echo You can:
echo 1. Run DrivingSchoolApp.exe from the deploy folder
echo 2. Zip the deploy folder for distribution
echo 3. Copy the deploy folder to any Windows PC (no Qt installation needed)
echo.
echo Press any key to open the deploy folder...
pause
explorer deploy
