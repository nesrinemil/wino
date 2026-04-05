# Quick Start Guide - Windows

## For Developers (Building from Source)

### Step 1: Install Qt
1. Download Qt installer from: https://www.qt.io/download-qt-installer
2. During installation, select:
   - Qt 6.5 or later (or Qt 5.15)
   - MinGW 64-bit compiler
   - Qt Creator IDE

### Step 2: Open Project
1. Launch Qt Creator
2. Open `DrivingSchoolApp.pro`
3. Select your Qt kit when prompted
4. Click "Configure Project"

### Step 3: Build & Run
1. Click the green play button or press Ctrl+R
2. The application will compile and launch in full screen

**That's it!** The application is now running.

---

## For End Users (Pre-built Executable)

If you received a pre-built version:

1. Extract the zip file to any folder
2. Open the folder
3. Double-click `DrivingSchoolApp.exe`
4. The application opens in full screen automatically

**No installation required!** All dependencies are included.

---

## Using the Application

### First Launch
- The role selection screen appears
- Choose your role: Admin, Instructor, or Student

### Student Dashboard
- Search for driving schools
- Filter by region and city
- View school details and vehicles
- Send registration requests

### Admin Dashboard
- View school approval requests
- Approve or reject schools
- Manage school status
- View school details

### Instructor Dashboard
- View available vehicles
- Check vehicle status
- Access vehicle information

### Navigation
- Use the **Logout** button to return to role selection
- All dashboards open in full screen for best experience

---

## Troubleshooting

**Problem:** Application won't start
- **Solution:** Make sure all DLL files are in the same folder as the .exe

**Problem:** Database errors
- **Solution:** Ensure the folder has write permissions

**Problem:** Window is too small
- **Solution:** The app should open maximized automatically. If not, click the maximize button.

---

## Data Storage

- Database: `driving_school.db` (created automatically in app folder)
- Sample data is pre-loaded for testing
- Data persists between sessions

---

## System Requirements

- Windows 10 or later (64-bit)
- 4GB RAM minimum
- 50MB disk space
- Screen: 1024x768 minimum (1920x1080 recommended)

---

**Need Help Building?**
See `WINDOWS_BUILD.md` for detailed build instructions.
