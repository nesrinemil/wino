# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# Wino Driving School Project Reference

> **Living document.** Every time a new feature is added or a pattern changes, update this file.
> Future Claude sessions must read this file first before touching any code.

---

## 1. Project Identity

| Field | Value |
|---|---|
| App name (UI) | **Wino** / **Maryem** |
| Project root | `C:/Users/hboug/OneDrive/Desktop/maryem/` |
| Qt version | **6.7.3** |
| C++ standard | **C++17** |
| Compiler | **MinGW 64-bit** (`C:/Qt/Tools/mingw1120_64/bin/`) |
| OS | Windows 11 |
| Git branch | `master` |
| Oracle DB | XE 11g, workspace `MARYEM_DRIVE`, user `ADMIN`, password `oracle123` |
| SQLite DB | Local, managed by `ParkingDBManager` singleton |

---

## 2. Build System

### Project file
```
C:/Users/hboug/OneDrive/Desktop/maryem/DrivingSchoolApp.pro
```

### Qt modules used
```
core gui sql network multimedia multimediawidgets svg printsupport serialport texttospeech widgets
```

### Key .pro variables
```qmake
SDS  = C:/Users/hboug/Downloads/final/SmartDrivingSchool/SmartDrivingSchool
WINO = wino
PARKING = parking
```

### Compile command (always use Qt MinGW, NOT system GCC)
```bash
export PATH="/c/Qt/Tools/mingw1120_64/bin:$PATH"
cd /c/Users/hboug/OneDrive/Desktop/maryem
mingw32-make -f Makefile.Debug
```

### Regenerate Makefile (if missing or stale)
```bash
export PATH="/c/Qt/Tools/mingw1120_64/bin:/c/Qt/6.7.3/mingw_64/bin:$PATH"
cd /c/Users/hboug/OneDrive/Desktop/maryem
qmake DrivingSchoolApp.pro -spec win32-g++ CONFIG+=debug
```

### Force recompile a file
```bash
touch parking/parkingwidget.cpp && mingw32-make -f Makefile.Debug
```

### Output binary
```
debug/DrivingSchoolApp.exe
```

> ⚠️ **ALWAYS close the running app before building** — Windows locks the .exe file.

---

## 3. Folder & File Map

```
maryem/                          ← project root
├── DrivingSchoolApp.pro
├── CLAUDE.md                    ← THIS FILE
├── main.cpp
├── mainwindow.cpp/.h            ← login window
├── admindashboard.cpp/.h        ← Add School wizard (admin panel)
├── studentdashboard.cpp/.h
├── instructordashboard.cpp/.h   ← outer shell, QTabWidget
├── studentlearninghub.cpp/.h    ← embeds WinoStudentDashboard
├── wino/                        ← booking + sessions + AI module
│   ├── thememanager.h/.cpp      ← singleton, dark/light theme
│   ├── wino_studentdashboard.h/.cpp   ← student session page
│   ├── wino_instructordashboard.h/.cpp
│   ├── adminwidget.h/.cpp       ← admin panel (11 pages)
│   ├── airecommendations.h/.cpp ← AI widget (not dialog)
│   ├── bookingsession.h/.cpp
│   ├── weatherservice.h/.cpp
│   ├── connection.h/.cpp        ← Oracle connection helper
│   └── wino_bootstrap.h        ← Oracle CREATE TABLE reference
├── parking/                     ← parking simulator module
│   ├── parkingwidget.h/.cpp     ← main parking widget (HUGE)
│   ├── parkingdbmanager.h/.cpp  ← SQLite singleton
│   ├── sensormanager.h/.cpp
│   └── statisticswidget.h/.cpp
└── ../9/                        ← code/theory learning module
    ├── codelearningmodule.h/.cpp
    ├── databasemanager.h/.cpp
    └── ...
```

### SmartDrivingSchool (Circuit module)
```
C:/Users/hboug/Downloads/final/SmartDrivingSchool/SmartDrivingSchool/
├── studentportal.cpp/.h         ← Exam Readiness Score, student portal
├── circuitdashboard.cpp/.h
└── ...
```

---

## 4. Architecture & Navigation

### WinoStudentDashboard — QStackedWidget (`m_mainStack`)
| Index | Page | How to navigate |
|---|---|---|
| 0 | Sessions dashboard (main) | `m_mainStack->setCurrentIndex(0)` |
| 1 | AI Recommendations | `m_mainStack->setCurrentIndex(1)` — page is `m_aiPage` |
| 2 | Exam request page | `m_mainStack->setCurrentIndex(2)` — page is `m_examPage` |

### ParkingWidget — QStackedWidget (`m_stack`)
| Index | Page |
|---|---|
| 0 | Vehicle selection (skipped on load) |
| 1 | Maneuver picker (home) |
| 2 | Active session (training interface) |
| 3 | Reservation / Oracle exam info |
| 4 | Analytics & Smart Coach |
| 5 | AI Assistant |

### AdminWidget — `m_pages` (11 pages, 0–10)
- Pages 1,2,3,4,5,7 → hidden for Moniteur role
- Page 10 → shown ONLY for Moniteur
- Nav buttons: `btn->setProperty("pageIndex", pageIdx)` — **never** use loop index

---

## 5. Database Patterns

### ✅ ALWAYS — Oracle query syntax
```cpp
QSqlQuery q(QSqlDatabase::database());  // explicit default connection
q.prepare("SELECT ... FROM TABLE WHERE col = :param");
q.bindValue(":param", value);
if (q.exec() && q.next()) {
    int val = q.value(0).toInt();
}
```

### ❌ NEVER — bare QSqlQuery (uses wrong connection)
```cpp
QSqlQuery q;   // WRONG — may pick SQLite instead of Oracle
```

### Oracle table names (UPPERCASE)
```sql
WINO_PROGRESS, STUDENTS, SESSION_BOOKING, DRIVING_SESSIONS,
SESSION_ANALYSIS, RECOMMENDATIONS, EXAM_REQUEST, EXAM_SESSIONS,
EXAM_REGISTRATIONS, AVAILABILITY, INSTRUCTORS, PAYMENT, CARS
```

### WINO_PROGRESS columns
```
user_id, current_step (1=Code/2=Circuit/3=Parking),
code_score, circuit_score, parking_score,
code_status, circuit_status, parking_status
```

### STUDENTS.role values
```
'Student'  'Instructor'  'Moniteur'  'Admin'
```
> ⚠️ No 'admin instructor' concept — only regular instructors with name/email/D17 ID.

### SQLite (ParkingDBManager)
```cpp
// Always access via singleton:
ParkingDBManager::instance().someMethod();
// Tables: vehicles, sessions, results, exam_sessions, exam_results, steps, reservations, videos
```

---

## 6. Current User Identity

```cpp
// Read current user ID (set by studentlearninghub.cpp or instructordashboard.cpp)
int userId = qApp->property("currentUserId").toInt();
QString role = qApp->property("currentUserRole").toString();

// Set it (done at login/switch):
qApp->setProperty("currentUserId", m_studentId);
```

---

## 7. ThemeManager Patterns

### Get instance and check theme
```cpp
ThemeManager* tm = ThemeManager::instance();
bool isDark = (tm->currentTheme() == ThemeManager::Dark);
```

### Color variables pattern (use in any refreshable function)
```cpp
ThemeManager* tm = ThemeManager::instance();
bool isDark      = tm->currentTheme() == ThemeManager::Dark;
QString C_BG      = tm->backgroundColor();
QString C_CARD    = tm->cardColor();
QString C_SURFACE = tm->surfaceColor();
QString C_TEXT    = tm->primaryTextColor();
QString C_MUTED   = tm->secondaryTextColor();
QString C_BORDER  = tm->borderColor();
QString C_TEAL    = tm->accentColor();
```

### Available color getters
```cpp
tm->backgroundColor()      // page background
tm->surfaceColor()         // slightly elevated surface
tm->cardColor()            // card/widget background
tm->primaryTextColor()     // main text
tm->secondaryTextColor()   // muted/subtitle text
tm->accentColor()          // teal accent (#00b894 light / teal dark)
tm->accentHoverColor()     // hover state of accent
tm->borderColor()          // dividers, card borders
tm->headerColor()          // top bar background
tm->headerTextColor()      // top bar text
```

### React to theme changes
```cpp
connect(ThemeManager::instance(), &ThemeManager::themeChanged,
        this, [this](ThemeManager::Theme) {
    refreshMyPage();   // re-apply all colors
});
```

### Theme-aware page pattern
1. Store shell widget pointers as members (`m_topBar`, `m_scrollArea`, `m_bodyWidget`)
2. Call `refreshMyPage()` at construction end AND in `themeChanged` signal
3. `refreshMyPage()` reads colors fresh from ThemeManager every call

---

## 8. UTF-8 String Rules (MinGW/GCC)

### ✅ Always wrap non-ASCII in fromUtf8
```cpp
QLabel *l = new QLabel(QString::fromUtf8("Démarrer l'examen"));
```

### Hex escape — greedy parsing trap
```cpp
// ❌ WRONG — GCC reads \xc3\xa9el as \xc3\xa9e + 'l' (error)
"\xc3\xa9el"

// ✅ CORRECT — split the literal
"\xc3\xa9" "el"
// OR use the actual character in fromUtf8:
QString::fromUtf8("éel")
```

### Common UTF-8 sequences
```cpp
"\xc3\xa9"           // é
"\xc3\xa8"           // è
"\xc3\xa0"           // à
"\xc2\xb0"           // °
"\xf0\x9f\x9a\x80"  // 🚀
"\xe2\x86\x90"       // ←
"\xe2\x97\x8f"       // ●
"\xf0\x9f\x94\x97"  // 🔗
"\xf0\x9f\x94\x98"  // 🔘
```

---

## 9. ParkingWidget Design System

### Static helpers (top of parkingwidget.cpp)
```cpp
static const char* BG = "background:#f0f2f5;";

static const char* SCROLL_SS =
    "QScrollArea{border:none;background:#f0f2f5;}"
    "QScrollBar:vertical{width:6px;background:transparent;}"
    "QScrollBar::handle:vertical{background:#d0d5db;border-radius:3px;min-height:30px;}"
    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}";

// Drop shadow helper
static QGraphicsDropShadowEffect* shadow(QWidget *p, int blur=20, int a=18){
    auto *s = new QGraphicsDropShadowEffect(p);
    s->setBlurRadius(blur); s->setColor(QColor(0,0,0,a)); s->setOffset(0,4);
    return s;
}

// White card with border
static QString cardSS(const QString &id){
    return QString("QFrame#%1{background:white;border-radius:16px;border:1px solid #e2e8f0;}").arg(id);
}
```

### Card pattern
```cpp
QFrame *card = new QFrame(this);
card->setObjectName("myCard");
card->setStyleSheet(cardSS("myCard"));
card->setGraphicsEffect(shadow(card));
QVBoxLayout *l = new QVBoxLayout(card);
l->setContentsMargins(18, 14, 18, 14);
l->setSpacing(8);
```

### Gradient card (Full ATTT Exam style)
```cpp
card->setStyleSheet(
    "QFrame#feCard{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
    "stop:0 #6c5ce7,stop:1 #a29bfe);"
    "border-radius:20px;border:none;}");
```

### Dark glass panel (Arduino monitor style)
```cpp
panel->setStyleSheet(
    "QFrame#panel{"
    "  background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #1e1e2e,stop:1 #2d2b55);"
    "  border-radius:16px;border:1px solid rgba(108,92,231,0.4);}");
```

---

## 10. Arduino HC-05 Bluetooth (QSerialPort)

### ⚠️ Port assignments — HARDCODED, never scan
| Port | Device | Usage |
|------|--------|-------|
| **COM3** | HC-05 Bluetooth (SPP) | Sensor data, step auto-advance |
| **COM6** | Arduino Uno (USB) | LCD timer display only |

**Never swap these.** `connectMaquette()` hardcodes `"COM3"`. `connectLcdPort()` hardcodes COM6 as first preference.

### Two separate serial handles
1. **`m_sensorMgr`** (`SensorManager`) — owns `COM3`. Emits parsed signals + `rawLineReceived` for log display.
2. **`m_lcdPort`** (`QSerialPort*`) — owns `COM6`. Write-only timer mirror.

> ⚠️ **Never open COM3 twice.** The old `m_arduinoSerial` second-open was removed (caused "Access is denied"). Raw lines now reach `appendArduinoMessage()` via `SensorManager::rawLineReceived` signal.

### HC-05 connection with auto-retry
```cpp
void ParkingWidget::connectMaquette()
{
    const QString HC05_PORT = "COM3";
    bool ok = m_sensorMgr->connectToPort(HC05_PORT);
    if (ok) {
        m_maquetteConnected = true;
        stopHc05Retry();
        onConnectionChanged(true);
    } else {
        // Port busy → show yellow "Retrying..." badge and retry every 5s
        startHc05Retry();
    }
    retryLcdConnect();   // also refresh LCD port
}
```

### Member variables (parkingwidget.h)
```cpp
// Full ATTT Exam card (home page)
QSerialPort  *m_arduinoSerial   = nullptr;   // used only by Full ATTT port combo
QTextEdit    *m_arduinoLog      = nullptr;   // log on home card
QPushButton  *m_btConnectBtn    = nullptr;
QComboBox    *m_btPortCombo     = nullptr;
QLabel       *m_btStatusLbl     = nullptr;
QString       m_arduinoBuffer;               // line accumulation buffer

// Session page (below car diagram in step card)
QTextEdit    *m_sessionArduinoLog = nullptr; // mirrored log in session page

// HC-05 auto-retry
QTimer *m_hc05RetryTimer = nullptr;          // retries COM3 every 5s when locked

// Maquette connection state
bool m_maquetteConnected = false;
QLabel *m_connStatus     = nullptr;          // badge: 🟢 Connected / 🔴 Failed / 🔄 Retrying
```

### Arduino State Machine (parking/Arduino firmware)
```
FORWARD       → sendMsg("STEP 1: MOVING FORWARD\nApproach barrier...")
              → when FRONT <= 28cm × 3: sendMsg("!! STOP !!\nTURN RIGHT + GO FORWARD...")
                → state = TURN_LEFT

TURN_LEFT     → waits 4s, then prints "STEP 2: R:Xcm" every loop (Serial.print, NOT sendMsg)
              → when RIGHT < 25cm × 3: sendMsg("GO STRAIGHT\nWatch right sensor...")
                → state = STEP3_FORWARD

STEP3_FORWARD → sendMsg("STEP 3: GO STRAIGHT\nLooking for barrier 4...")
              → when barrier4Seen + rightLost × 4: sendMsg("!! EXTREMITY REACHED !!\nSTOP\n>> REVERSE + WHEEL RIGHT <<")
                → state = REV_RIGHT

REV_RIGHT     → sendMsg("PHASE 4: REVERSE + WHEEL RIGHT\nKeep reversing...")
              → when RIGHT < 20cm AND BACK_R 9-17cm × 5: sendMsg("!! POSITION OK !!\nSTOP\n>> REVERSE + WHEEL LEFT <<")
                → state = REV_LEFT

REV_LEFT      → sendMsg("PHASE 5: REVERSE + WHEEL LEFT\nStraightening...")
              → when BACK_L 2-5cm × 4: sendMsg("✅ PARKING COMPLETE!\nBL:X BR:X")
                → state = PARKED

PARKED        → sendMsg("✅ PARKING COMPLETE!\nF:X BL:X BR:X") [repeating, but deduplicated]
```

**Key:** `sendMsg()` deduplicates — each message prints only ONCE (Arduino's `lastMsg` guard).  
`"STEP 2: R:Xcm"` is a raw `Serial.print` in the loop — repeats every 100ms but debounce blocks.

### Qt ↔ Arduino auto-advance mapping
| Arduino sends | Qt action | Debounce |
|---|---|---|
| `STEP 1:` (e.g. "STEP 1: MOVING FORWARD") | `advanceStep()` after 1.5s | `m_arduinoAdvancePending` |
| `!! STOP !!` | `advanceStep()` after 1.5s | same |
| `GO STRAIGHT` | `advanceStep()` after 1.5s | same |
| `EXTREMITY REACHED` (in any case) | `advanceStep()` after 1.5s | same |
| `POSITION OK` (in any case) | `advanceStep()` after 1.5s | same |
| `PHASE 4:` (fallback if primary missed) | `advanceStep()` after 1.5s | same |
| `PHASE 5:` (fallback if primary missed) | `advanceStep()` after 1.5s | same |
| `PARKING COMPLETE` | `endSession()` after 2s | same |

### Debounce members (parkingwidget.h)
```cpp
bool m_arduinoAdvancePending = false; // true while QTimer for advanceStep() is pending
bool m_sessionEndHandled     = false; // true once endSession() ran (prevents double DB save)
```
Reset: `m_arduinoAdvancePending` in `advanceStep()` and `endSession()`.  
Reset: `m_sessionEndHandled` in `startSession()`.

### Message color-coding (appendArduinoMessage)
| Prefix/Content | Color | Auto-advance? | Spoken? |
|---|---|---|---|
| `PARKING COMPLETE` | `#55efc4` gold | endSession | ✅ |
| `!! POSITION OK !!` | `#00b894` green | ✅ advance | ✅ |
| `PHASE 5:` | `#a29bfe` purple | ✅ advance (fallback) | ✅ |
| `PHASE 4:` | `#fdcb6e` orange | ✅ advance (fallback) | ✅ |
| `!! EXTREMITY REACHED !!` | `#ff7675` red | ✅ advance | ✅ |
| `GO STRAIGHT` | `#00cec9` teal | ✅ advance | ✅ |
| `STEP 3:` | `#74b9ff` blue | — display only | ✅ |
| `STEP 2:` | `#74b9ff` blue dim | — display only | ✅ |
| `STEP 1:` | `#00cec9` teal | ✅ advance | ✅ |
| `!! STOP !!` | `#ff7675` red | ✅ advance | ✅ |
| `>> ...` (sub-instructions) | `#fdcb6e` orange | — | ✅ |
| `TURN RIGHT/LEFT/REVERSE` | `#fdcb6e` yellow | — | ✅ |
| `confirmed:` / `wait:` / `count:` | `#636e72` dim italic | — | ❌ |
| `F:` `R:` `BR:` `BL:` sensor | `#636e72` dim small | — | ❌ |
| `Calibrating`/`Gyro`/`Waking`/`Lost count` | `#b2bec3` italic | — | ❌ |

### Message mirroring
- `appendArduinoMessage()` writes to **both** `m_arduinoLog` (home card) AND `m_sessionArduinoLog` (session page)
- Auto-scrolls both logs to bottom

### Auto-advance on Arduino events
```cpp
// "Step X confirmed" → advance training step after 1.2s (voice plays first)
if (line.startsWith("Step") && line.contains("confirmed")) {
    QTimer::singleShot(1200, this, [this]() {
        if (m_sessionActive) advanceStep();
    });
}
// "PARKING COMPLETE" → end session after 2s
if (line.contains("PARKING COMPLETE")) {
    QTimer::singleShot(2000, this, [this]() {
        if (m_sessionActive) endSession();
    });
}
```

### Connect Model button location
- **Full ATTT Exam card** (home page): has port combo + connect button → manages `m_arduinoSerial` (separate port, not COM3 if already owned by m_sensorMgr)
- **Session page / Sensors section**: "Connect Model" button calls `connectMaquette()` only → `m_sensorMgr` handles COM3

### SensorManager signals (sensormanager.h)
```cpp
void rawLineReceived(const QString &line);      // every raw line → fed to appendArduinoMessage()
void arduinoStepAdvance(const QString &step);   // "STEP1_DONE" / "STEP4_DONE" / "PARKING_DONE"
void connected(const QString &port);
void disconnected();
void connectionError(const QString &error);     // deduplicated: same message suppressed for 3s
```

### SensorManager error deduplication (sensormanager.cpp)
```cpp
// Suppresses duplicate "Access is denied" (Qt fires 2+ error codes for same OS error)
static qint64  lastShownMs = 0;
static QString lastMsg;
qint64 now = QDateTime::currentMSecsSinceEpoch();
if (msg == lastMsg && (now - lastShownMs) < 3000) return;
```

### Créneau — simplified to 3 auto-advancing steps
| Step | Phase tag | Arduino signal | Trigger string |
|------|-----------|----------------|----------------|
| 1 — Align alongside | `"ALIGN"` | `STEP1_DONE` | "TURN RIGHT + GO FORWARD" |
| 2 — Counter-steer LEFT | `"TURN_LEFT"` | `STEP4_DONE` | "POSITION OK" |
| 3 — Final Park (auto-ends) | `"STRAIGHT"` | `PARKING_DONE` | "PARKING COMPLETE" |

Step 3 has **no Next button** — session ends automatically via `QTimer::singleShot(1500, endSession)`.

### Fwd Corridor → Créneau auto-chain
When maneuver index 5 (Fwd Corridor) ends in non-full-exam mode, `endSession()` automatically opens Créneau (index 0) after 1.8s:
```cpp
if (m_currentManeuver == 5 && !m_fullExamMode) {
    QTimer::singleShot(1800, this, [this]{ openManeuver(0); });
    return;  // don't show results dialog
}
```

---

## 11. Voice / TTS (QTextToSpeech)

### Setup (in ParkingWidget constructor)
```cpp
m_tts = new QTextToSpeech(this);
m_voiceEnabled = true;
// Set English locale
for (const auto &locale : m_tts->availableLocales()) {
    if (locale.language() == QLocale::English) {
        m_tts->setLocale(locale);
        break;
    }
}
m_tts->setRate(0.0);    // normal speed
m_tts->setVolume(0.9);
```

### Speak a line
```cpp
void ParkingWidget::speak(const QString &text) {
    if (!m_voiceEnabled || !m_tts) return;
    if (m_tts->state() == QTextToSpeech::Speaking)
        m_tts->stop();
    m_tts->say(text);
}
```

### Sensor warning cooldown
```cpp
m_sensorSpeakCooldown = new QTimer(this);
m_sensorSpeakCooldown->setSingleShot(true);
// In speakSensorWarning(): check isActive() first → 3s cooldown
```

---

## 11b. LCD Arduino 2 (COM6 USB) — Timer Mirror

### Purpose
A second Arduino Uno connected via USB (COM6) displays the parking session timer on a 16×2 I²C LCD (address `0x24`, `LiquidCrystal_PCF8574`).

### Protocol (9600 baud, `\n`-terminated, Qt → Arduino)
| Command | Arduino action |
|---------|----------------|
| `START` | Clear LCD, show `00:00` / `Step --` |
| `T:MM:SS` | Update line 0 with timer |
| `S:N/T` | Update line 1 with `Step N/T` |
| `DONE` | Show `PARKING` / `COMPLETE!` |
| `RESET` | Show `WAITING...` (idle) |

### LCD member variables (parkingwidget.h)
```cpp
QSerialPort *m_lcdPort   = nullptr;  // COM6 write-only handle
QLabel      *m_lcdStatus = nullptr;  // badge: 🔴 LCD / 🟢 LCD
```

### Key functions
```cpp
void connectLcdPort();       // tries COM6 first, then USB description fallback
void retryLcdConnect();      // force-close then reopen (called by connectMaquette)
void sendToLcd(const QString &cmd);   // write + flush; updates badge to 🔴 if port lost
```

### RESET timing rule
`RESET` is only sent if **no session is active** (avoids wiping live timer):
```cpp
QTimer::singleShot(2000, this, [this]{ if (!m_sessionActive) sendToLcd("RESET"); });
```

### LCD startup sequence (after COM6 opens)
1. t+2s → `RESET` (if no active session)
2. t+3.5s → `RESET` (if no active session)
3. t+4s → if session active: `START` + `S:N/T` + `T:MM:SS` resync; else `RESET`

### Arduino 2 sketch (`parking/lcd_timer/lcd_timer.ino`)
- Merged with existing RFID + Servo + IR sensor code
- `readSerial()` called at top of every `loop()` — non-blocking
- `restoreTimer()` called after each RFID/IR LCD write (restores timer display)
- `timerActive = true` is set inside the `T:` handler (auto-activates even if `START` was missed)

### LCD is detected automatically at app startup (+600ms) and on every "Connect Model" click.

---

## 12. Full ATTT Exam Flow

```
openFullExam()
  └─ sets m_fullExamMode = true, m_fullExamPhase = 0
  └─ opens maneuver sequence: FULL_EXAM_SEQ = {5, 0, 4, 3}
       5=Fwd Corridor, 0=Créneau, 4=Demi-tour, 3=Marche AR

openManeuver(idx)
  └─ m_stack->setCurrentIndex(2)  ← goes to session page
  └─ if fullExamMode && phase > 0 → skip step 0 (safety checks)

finishSession()
  └─ if fullExamMode && phase < 3 → m_fullExamPhase++, chain next
  └─ if fullExamMode && phase == 3 → showFullExamResults()
```

### Full exam always unlocked (no booking required)
```cpp
void ParkingWidget::updateFullExamCard() {
    m_fullExamStartBtn->setVisible(true);
    m_fullExamLockLabel->setVisible(false);
}
```

---

## 13. WinoStudentDashboard Exam Page

### createExamPage() — member pointers to save
```cpp
m_examTopBar     // QWidget* — top bar of exam page (theme updates)
m_examScroll     // QScrollArea* — scroll area (theme updates)
m_examBodyWidget // QWidget* — inner content widget (theme updates)
m_examBodyLayout // QVBoxLayout* — refreshed by refreshExamPage()
```

### refreshExamPage() — called on construction + themeChanged
```cpp
void WinoStudentDashboard::refreshExamPage() {
    if (!m_examBodyLayout) return;
    // 1. Get fresh ThemeManager colors (C_BG, C_CARD, etc.)
    // 2. Update m_examTopBar, m_examScroll, m_examBodyWidget styles
    // 3. Delete all children of m_examBodyLayout
    // 4. Rebuild all content with new colors
}
```

### Exam eligibility logic
- Minimum sessions required per step: Code=10, Circuit=8, Parking=5
- Shows "X more sessions needed" banner if not eligible
- Pulls from `SESSION_BOOKING` WHERE `status='COMPLETED'`

---

## 14. Add School Wizard (admindashboard.cpp)

### Stack structure (after removing admin instructor)
```
Stack index 0: School info page
Stack index 1+i: Instructor i page (i = 0..N-1)
```

### totalSteps lambda
```cpp
auto totalSteps = [&]() { return 1 + instrCountSpin->value(); };
```

### refreshLabels() — step numbers start at 1
```cpp
// School page = step 1
// Instructor i = step (i+2)
```

### doSubmit() — only inserts instructors, no admin
```cpp
// INSERT INTO STUDENTS (login, password, full_name, role, ...) for each instructor
// role = 'Instructor'  (never 'Admin')
```

---

## 15. Code Learning Module (codelearningmodule.cpp)

### Dark/light theme fix — MUST call at end of constructor
```cpp
// After all setup (initializeSections, setupUI, refresh* calls):
setDarkMode(m_isDarkMode);
```
> Without this, the module always opens in dark mode regardless of app theme.

---

## 16. Exam Readiness Score Fix (studentportal.cpp)

### Guard against false 50% when no sessions
```cpp
double readiness = 0.0;
if (totalSessions > 0) {
    readiness = qMin(100.0,
        (passRate * 50.0)
        + ((1.0 - avgStress) * 25.0)
        + ((1.0 - avgRisk)   * 25.0));
}
if (totalSessions == 0)
    advice = "No sessions recorded yet. Complete driving sessions...";
```

---

## 17. Coding Rules (Mandatory)

| Rule | Detail |
|---|---|
| **Edit tool only** | Never use bash `echo`/`cat`/`heredoc` for file edits — corrupts files |
| **Read before Edit** | Always `Read` the file before calling `Edit` |
| **QSqlQuery explicit** | Always `QSqlQuery q(QSqlDatabase::database())` |
| **UTF-8 non-ASCII** | Always `QString::fromUtf8("...")` for accented chars |
| **Compile after changes** | Build and fix errors before moving to next change |
| **Close app before build** | Windows locks the .exe |
| **No system GCC** | Use Qt MinGW: `export PATH="/c/Qt/Tools/mingw1120_64/bin:$PATH"` |
| **ThemeManager singleton** | `ThemeManager::instance()` — never instantiate directly |
| **AdminWidget nav** | Always set `btn->setProperty("pageIndex", pageIdx)` |

---

## 18. GitHub Remote

```
https://github.com/nesrinemil/wino.git
```

### Push all changes
```bash
cd /c/Users/hboug/OneDrive/Desktop/maryem
git add -A
git commit -m "your message"
git push origin master
```

---

## 19. Modification Log

> **Add a new entry here every time a feature is implemented or a bug is fixed.**

| Date | What changed | File(s) |
|---|---|---|
| 2026-04-10 | Sessions page redesign, AI page embedded in m_mainStack | `wino_studentdashboard.cpp` |
| 2026-04-10 | Admin panel role-based nav (Moniteur), Page 10 Parking students | `adminwidget.cpp` |
| 2026-04-10 | ParkingWidget auto vehicle selection, removed Change Vehicle button | `parkingwidget.cpp` |
| 2026-04-10 | Exam embedded page (index 2 in m_mainStack) | `wino_studentdashboard.cpp` |
| 2026-05-04 | Removed admin instructor from Add School wizard | `admindashboard.cpp` |
| 2026-05-04 | Code section always-dark fix — call `setDarkMode()` at end of constructor | `codelearningmodule.cpp` |
| 2026-05-04 | Exam Readiness Score false 50% fix (guard if totalSessions==0) | `studentportal.cpp` |
| 2026-05-04 | Exam page full theme support (refreshExamPage + themeChanged signal) | `wino_studentdashboard.cpp/.h` |
| 2026-05-04 | Full ATTT Exam always unlocked (removed booking requirement) | `parkingwidget.cpp` |
| 2026-05-04 | Arduino HC-05 monitor added to Full ATTT Exam card | `parkingwidget.cpp/.h` |
| 2026-05-04 | Arduino messages voiced via TTS (speak() in appendArduinoMessage) | `parkingwidget.cpp` |
| 2026-05-04 | Arduino log mirrored in session page below 3D car view | `parkingwidget.cpp/.h` |
| 2026-05-04 | "Connect Model" in Sensors section also connects HC-05 serial | `parkingwidget.cpp` |
| 2026-05-04 | Arduino auto-advances Next step on "Step X confirmed"; auto-ends on "PARKING COMPLETE" | `parkingwidget.cpp` |
| 2026-05-04 | Fixed case-insensitive Arduino step detection (toLower); fixed Previous/Next/Finish hidden after auto-advance | `parkingwidget.cpp` |
| 2026-05-04 | Fixed Arduino auto-advance: changed trigger from "Step X confirmed" (never sent) to actual Arduino phase headers (STEP 2:, STEP 3:, PHASE 4:, PHASE 5:). Added m_arduinoAdvancePending debounce to prevent double-advance | `parkingwidget.cpp/.h` |
| 2026-05-04 | Last step "Complete" button: rewires Next→endSession() directly in updateStepUI() so it always works even if m_sessionActive was already false | `parkingwidget.cpp` |
| 2026-05-04 | Added m_sessionEndHandled boolean guard in endSession() to prevent double DB save; reset in startSession() | `parkingwidget.cpp/.h` |
| 2026-05-04 | Session Arduino log cleared on each new session start (fixes hint spam accumulation) | `parkingwidget.cpp` |
| 2026-05-04 | Per-step guidance in updateStepUI(): PREP→blue manual hint, DONE→green complete hint, others→green auto hint | `parkingwidget.cpp` |
| 2026-05-04 | Full Arduino protocol integration: appendArduinoMessage() rewritten to match exact state machine — STEP 1:→advance, !!STOP!!→advance, GO STRAIGHT→advance, !!EXTREMITY REACHED!!→advance, !!POSITION OK!!→advance, PARKING COMPLETE→endSession(). Color-coded log + TTS filtering | `parkingwidget.cpp` |
| 2026-05-04 | startSession() guidance shows "Étape manuelle" for PREP steps instead of generic "Let's go". Hint text updated with real trigger messages | `parkingwidget.cpp` |
| 2026-05-05 | Créneau reduced from 7 steps to 3 auto-advancing steps (ALIGN→STEP1_DONE, TURN_LEFT→STEP4_DONE, STRAIGHT→PARKING_DONE auto-ends) | `parkingwidget.cpp` |
| 2026-05-05 | Fwd Corridor "Complete" auto-chains to Créneau (no dialog, 1.8s delay) in non-full-exam mode | `parkingwidget.cpp` |
| 2026-05-05 | LCD Arduino 2 (COM6) integrated: connectLcdPort/sendToLcd/retryLcdConnect, timer mirrored every second, DONE on session end | `parkingwidget.cpp/.h` |
| 2026-05-05 | Arduino 2 sketch created: merged LCD timer with RFID/Servo/IR code, readSerial() non-blocking, restoreTimer() after events | `parking/lcd_timer/lcd_timer.ino` |
| 2026-05-05 | HC-05 hardcoded to COM3 in connectMaquette() — stops accidental COM6 grab | `parkingwidget.cpp` |
| 2026-05-05 | HC-05 auto-retry: if COM3 locked, retries every 5s silently, shows 🔄 badge | `parkingwidget.cpp/.h` |
| 2026-05-05 | Removed duplicate m_arduinoSerial COM3 open from Connect Model button — was causing "Access is denied" | `parkingwidget.cpp` |
| 2026-05-05 | rawLineReceived + arduinoStepAdvance signals added to SensorManager; parseLine() updated | `sensormanager.h/.cpp` |
| 2026-05-05 | SensorManager error dedup: timestamp-based 3s cooldown (Qt fires multiple error codes for same OS error) | `sensormanager.cpp` |
| 2026-05-05 | LCD status badge 🔴/🟢 added next to Connect Model button in Sensors card | `parkingwidget.cpp` |
| 2026-05-05 | LCD RESET suppressed during active sessions to prevent timer wipe on COM6 reconnect | `parkingwidget.cpp` |
