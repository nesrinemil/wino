# Building for Windows - Driving School Management System

## Prerequisites

### 1. Install Qt for Windows
Download and install Qt from: https://www.qt.io/download-qt-installer

**Recommended Version:** Qt 6.5 or later (or Qt 5.15 if preferred)

**Required Components:**
- Qt Widgets
- Qt SQL
- MinGW 64-bit compiler (or MSVC 2019/2022)

### 2. Install Qt Creator (included with Qt installer)

## Building the Application

### Option 1: Using Qt Creator (Recommended)

1. **Open the Project:**
   - Launch Qt Creator
   - Click "File" → "Open File or Project"
   - Navigate to the extracted folder and select `DrivingSchoolApp.pro`
   - Click "Open"

2. **Configure Project:**
   - Qt Creator will ask you to configure the project
   - Select your installed Qt kit (MinGW or MSVC)
   - Click "Configure Project"

3. **Build:**
   - Click the green "Build" button (hammer icon) or press `Ctrl+B`
   - Wait for the build to complete

4. **Run:**
   - Click the green "Run" button (play icon) or press `Ctrl+R`
   - The application will launch in full screen

### Option 2: Using Command Line

1. **Open Qt Command Prompt:**
   - Start Menu → Qt → Qt 6.x.x (MinGW) → Qt 6.x.x (MinGW) Command Prompt
   - Or use "Developer Command Prompt for VS" if using MSVC

2. **Navigate to Project Directory:**
   ```cmd
   cd C:\path\to\DrivingSchoolApp
   ```

3. **Generate Makefile:**
   ```cmd
   qmake DrivingSchoolApp.pro
   ```

4. **Compile:**
   - For MinGW:
     ```cmd
     mingw32-make
     ```
   - For MSVC:
     ```cmd
     nmake
     ```

5. **Run:**
   ```cmd
   DrivingSchoolApp.exe
   ```

## Creating a Standalone Executable

To distribute the application without requiring Qt installation:

1. **Build in Release Mode:**
   - In Qt Creator, select "Release" from the build configuration dropdown
   - Build the project

2. **Find the Executable:**
   - The .exe file will be in the build folder (e.g., `build-DrivingSchoolApp-Desktop_Qt_6_x_x_MinGW_64_bit-Release\release\`)

3. **Deploy Qt Dependencies:**
   - Open Qt Command Prompt
   - Navigate to the folder containing your .exe
   - Run windeployqt:
     ```cmd
     cd C:\path\to\build\release
     windeployqt DrivingSchoolApp.exe
     ```
   - This will copy all necessary Qt DLLs to the folder

4. **The folder now contains:**
   - `DrivingSchoolApp.exe`
   - All required Qt DLLs
   - Database file (created on first run)

5. **Distribute:**
   - Zip the entire folder
   - Users can extract and run `DrivingSchoolApp.exe` without installing Qt

## Troubleshooting

### "Cannot find -lQt6Widgets"
- Ensure Qt is properly installed
- Check that the correct Qt kit is selected in Qt Creator

### "The program can't start because Qt6Core.dll is missing"
- Run `windeployqt` on your executable (see step 3 above)
- Or copy the .exe to the Qt bin directory for testing

### Database Errors
- The application automatically creates `driving_school.db` in the same folder as the .exe
- Ensure the folder has write permissions

### Application Doesn't Open in Full Screen
- This is normal - the application is designed to open maximized
- If it doesn't maximize, check your Windows display settings

## System Requirements

- **OS:** Windows 10 or later (64-bit)
- **RAM:** 4GB minimum, 8GB recommended
- **Screen Resolution:** 1024x768 minimum, 1920x1080 recommended
- **Disk Space:** 50MB for application + dependencies

## File Structure After Build

```
DrivingSchoolApp/
├── DrivingSchoolApp.exe          # Main executable
├── driving_school.db              # SQLite database (created on first run)
├── Qt6Core.dll                    # Qt libraries (after windeployqt)
├── Qt6Gui.dll
├── Qt6Widgets.dll
├── Qt6Sql.dll
└── [other Qt dependencies]
```

## Creating a Windows Installer (Optional)

You can use tools like:
- **Inno Setup** (Free): https://jrsoftware.org/isinfo.php
- **NSIS** (Free): https://nsis.sourceforge.io/
- **Qt Installer Framework** (Official Qt tool)

These will create a professional installer (.exe) that includes:
- Application files
- Start Menu shortcuts
- Desktop shortcut
- Uninstaller

## Additional Notes

- The application uses SQLite (included with Qt), no separate database installation needed
- Sample data is automatically populated on first run
- All windows open maximized for optimal viewing
- The database file persists between runs, maintaining all data

## Support

For build issues:
1. Check Qt installation is complete
2. Verify the correct Qt kit is selected
3. Clean and rebuild the project (Build → Clean All, then Build → Rebuild All)
4. Check the Qt documentation: https://doc.qt.io/

---

**Built with Qt Framework - Cross-platform Application Development**
