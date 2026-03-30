# Driving School Management System
## Wino - Module 2

A complete, modern, responsive driving school management application built with Qt C++.

## Features

### 🎨 Responsive Design
- Works on all computer screen sizes (from 1024x768 to 4K displays)
- Adaptive layouts that resize smoothly
- Modern, clean interface with professional styling

### 👥 Three User Roles

#### 1. **Student Dashboard** (Teal Theme)
- Search for driving schools by name, region, or city
- View school details including:
  - Ratings and reviews
  - Number of students and vehicles
  - Available vehicles with transmission types
  - Location information
- Send registration requests
- Real-time search filtering

#### 2. **Instructor Dashboard** (Orange Theme)
- View all available vehicles
- Check vehicle status (Available/Maintenance)
- View vehicle details (brand, model, year, plate number, transmission)
- Clean, card-based layout

#### 3. **Admin Dashboard** (Purple Theme)
- Review school approval requests
- Approve or reject driving schools
- View all schools with their current status
- Manage school registrations

### 🗄️ Database Features
- SQLite database for data persistence
- Automatic table creation on first run
- Sample data included for testing
- Supports:
  - Driving schools with detailed information
  - Vehicles with specifications
  - Instructors
  - Status tracking

## Installation & Setup

### Prerequisites
- Qt 6.x or Qt 5.15+ with Qt Widgets and Qt SQL modules
- C++17 compatible compiler
- SQLite support (included with Qt)

### Build Instructions

#### Using Qt Creator:
1. Open `DrivingSchoolApp.pro` in Qt Creator
2. Click "Configure Project"
3. Click the green "Run" button

#### Using Command Line:
```bash
# Generate Makefile
qmake DrivingSchoolApp.pro

# Build
make

# Run (Linux/Mac)
./DrivingSchoolApp

# Run (Windows)
DrivingSchoolApp.exe
```

## Usage Guide

### First Launch
1. The application will create a SQLite database file `driving_school.db`
2. Sample data is automatically populated
3. Role selection screen appears

### Navigation
1. **Select a Role**: Choose between Admin, Instructor, or Student
2. **Explore the Dashboard**: Each role has unique features
3. **Logout**: Use the logout button to return to role selection

### Student Features
- **Search**: Type in the search box to find schools by name
- **Filter by Region**: Select a specific region from the dropdown
- **Filter by City**: Select a specific city from the dropdown
- **Reset**: Click reset to clear all filters
- **View Details**: Click "View Details" on any school card to see:
  - Complete school information
  - All available vehicles
  - Option to send registration request

### Admin Features
- **Review Requests**: See all pending school approval requests
- **Approve**: Click the green "✓ Approve" button
- **Reject**: Click the red "✗ Reject" button
- **Status Tracking**: View approved, rejected, and pending schools

### Instructor Features
- **Vehicle Overview**: See all vehicles in the system
- **Status Check**: Identify available vs maintenance vehicles
- **Quick Reference**: Access vehicle specifications at a glance

## Database Schema

### driving_schools
- id (PRIMARY KEY)
- name
- location (region)
- city
- postal_code
- students_count
- vehicles_count
- rating
- status (pending/approved/rejected)

### vehicles
- id (PRIMARY KEY)
- school_id (FOREIGN KEY)
- brand
- model
- year
- plate_number
- transmission (Manual/Automatic)
- status (active/maintenance)

### instructors
- id (PRIMARY KEY)
- school_id (FOREIGN KEY)
- name
- email
- phone
- experience_years
- status

## Responsive Design Details

The application automatically adapts to different screen sizes:

- **Minimum Resolution**: 1024x768
- **Recommended**: 1920x1080 or higher
- **Supports**: Up to 4K displays (3840x2160)

### Responsive Features:
- Flexible layouts using Qt's layout managers
- Scalable fonts and icons
- Adaptive spacing and margins
- Scrollable content areas for smaller screens
- Maximized window option for full-screen experience

## Color Scheme

- **Student/Teal**: #14B8A6 (Primary), #0D9488 (Hover)
- **Instructor/Orange**: #F59E0B (Primary)
- **Admin/Purple**: #7C3AED (Primary)
- **Neutral Grays**: #F3F4F6 (Background), #1F1827 (Text), #6B7280 (Secondary Text)
- **Status Colors**:
  - Success/Approved: #D1FAE5 (Background), #065F46 (Text)
  - Warning/Pending: #FEF3C7 (Background), #92400E (Text)
  - Error/Rejected: #FEE2E2 (Background), #991B1B (Text)

## Project Structure

```
DrivingSchoolApp/
├── main.cpp                    # Application entry point
├── mainwindow.h/.cpp/.ui      # Main window (launches role selection)
├── database.h/.cpp            # Database initialization and management
├── roleselectionwindow.h/.cpp/.ui  # Role selection screen
├── studentdashboard.h/.cpp/.ui     # Student interface
├── instructordashboard.h/.cpp/.ui  # Instructor interface
├── admindashboard.h/.cpp/.ui       # Admin interface
└── DrivingSchoolApp.pro       # Qt project file
```

## Customization

### Adding More Sample Data
Edit `database.cpp` in the `insertSampleData()` function to add more:
- Driving schools
- Vehicles
- Instructors

### Changing Colors
Modify the `styleSheet` properties in the `.ui` files or in the constructor of each dashboard class.

### Adding Features
Each dashboard has clear separation of concerns:
- Header files define interfaces
- Implementation files contain logic
- UI files define layouts and styling

## Troubleshooting

### Database Won't Create
- Ensure write permissions in the application directory
- Check that Qt SQL module is installed: `QT += sql` in .pro file

### Display Issues
- Try running in maximized mode: `showMaximized()`
- Adjust the minimum window size in the .ui files
- Check Qt version compatibility

### Build Errors
- Verify Qt installation includes Qt Widgets and Qt SQL
- Ensure C++17 support in your compiler
- Check that all files are in the same directory

## Future Enhancements

Potential features to add:
- User authentication system
- Booking/scheduling for driving lessons
- Payment processing
- Email notifications
- Multi-language support
- Dark mode theme
- Export reports to PDF
- Advanced search filters
- Real-time availability updates

## License

This is an educational project created for learning purposes.

## Support

For issues or questions about this application, please refer to the Qt documentation at https://doc.qt.io/

---

**Built with Qt Framework** - Cross-platform application development made easy.
