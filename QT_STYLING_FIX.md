# Qt Styling Issues - Complete Fix Guide

## Problem Description
- Black bars appear as grey
- Text in comboboxes ("All regions", "All cities") not showing
- Search placeholder text not appearing
- Icons not rendering properly

## Root Cause
Missing Qt styling plugins and theme engines on your system.

---

## SOLUTION 1: Install Missing Qt Packages (RECOMMENDED)

### For Ubuntu/Debian (20.04, 22.04, 24.04):

```bash
# Update package list
sudo apt-get update

# Install Qt6 styling components
sudo apt-get install -y qt6-style-plugins
sudo apt-get install -y qgnomeplatform-qt6
sudo apt-get install -y qt6-gtk-platformtheme

# Install additional Qt modules
sudo apt-get install -y libqt6svg6
sudo apt-get install -y qt6-wayland

# Install icon themes
sudo apt-get install -y adwaita-qt6
sudo apt-get install -y breeze-icon-theme

# Install fonts
sudo apt-get install -y fonts-liberation
sudo apt-get install -y fonts-dejavu-core
```

### For Fedora/RHEL (37, 38, 39):

```bash
# Install styling
sudo dnf install -y qt6-qtstyleplugins
sudo dnf install -y adwaita-qt6

# Additional modules
sudo dnf install -y qt6-qtwayland
sudo dnf install -y qt6-qtsvg

# Fonts
sudo dnf install -y liberation-fonts
```

### For Arch Linux:

```bash
# Install all needed packages
sudo pacman -S qt6-styleplugins qt6-wayland qt6-svg adwaita-qt6
```

---

## SOLUTION 2: Set Environment Variables

Add these lines to `~/.bashrc` (or `~/.zshrc` if using zsh):

```bash
# Qt Environment Variables
export QT_QPA_PLATFORMTHEME=gtk3
export QT_STYLE_OVERRIDE=Fusion
export QT_AUTO_SCREEN_SCALE_FACTOR=1
export QT_FONT_DPI=96
```

Then reload your shell:
```bash
source ~/.bashrc
```

---

## SOLUTION 3: Code Fix (Already Applied)

The updated `main.cpp` now forces the Fusion style, which is built into Qt:

```cpp
QApplication a(argc, argv);
a.setStyle(QStyleFactory::create("Fusion"));
```

This ensures consistent appearance across all systems.

---

## How to Apply the Fixes

### Step 1: Install Packages
Run the commands for your Linux distribution (see above).

### Step 2: Extract the New Code
Extract the updated `DrivingSchoolApp_Final.zip` file.

### Step 3: Clean Build
```bash
cd DrivingSchoolApp
rm -rf build/
make clean  # if you have an old build
```

### Step 4: Rebuild
```bash
qmake DrivingSchoolApp.pro
make
```

### Step 5: Run
```bash
./DrivingSchoolApp
```

---

## Verify Qt Installation

Check what Qt styles are available:
```bash
# Test Qt styles
python3 -c "from PyQt6.QtWidgets import QStyleFactory; print(QStyleFactory.keys())"
```

Or create a test file `test_styles.cpp`:
```cpp
#include <QApplication>
#include <QStyleFactory>
#include <QDebug>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    qDebug() << "Available styles:" << QStyleFactory::keys();
    return 0;
}
```

Compile and run:
```bash
g++ test_styles.cpp -o test_styles $(pkg-config --cflags --libs Qt6Widgets)