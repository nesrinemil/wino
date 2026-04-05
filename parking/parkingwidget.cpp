#include "parkingwidget.h"
#include "statisticswidget.h"
#include <QDialog>
#include <QScrollArea>
#include <QScrollBar>
#include <QProcess>
#include <QTemporaryFile>
#include <QSettings>
#include <QCheckBox>
#include <QGraphicsDropShadowEffect>
#include <QMessageBox>
#include <QSqlRecord>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <QDialogButtonBox>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QResizeEvent>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QUuid>

// ═══════════════════════════════════════════════════════════════════
//  DESIGN SYSTEM — Professional UI (Wino Parking Module)
// ═══════════════════════════════════════════════════════════════════

static const char* BG = "background:#f0f2f5;";
static const char* SCROLL_SS =
    "QScrollArea{border:none;background:#f0f2f5;}"
    "QScrollBar:vertical{width:6px;background:transparent;}"
    "QScrollBar::handle:vertical{background:#d0d5db;border-radius:3px;min-height:30px;}"
    "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}";

static QGraphicsDropShadowEffect* shadow(QWidget *p, int blur=20, int a=18){
    auto *s=new QGraphicsDropShadowEffect(p);
    s->setBlurRadius(blur); s->setColor(QColor(0,0,0,a)); s->setOffset(0,4);
    return s;
}

static QString cardSS(const QString &id){
    return QString("QFrame#%1{background:white;border-radius:16px;border:1px solid #e2e8f0;}").arg(id);
}

void ParkingWidget::initManeuverData()
{
    m_maneuverNames = {
        QString::fromUtf8("Parallel"),
        QString::fromUtf8("Perpendicular"),
        QString::fromUtf8("Diagonal"),
        QString::fromUtf8("Reverse"),
        QString::fromUtf8("U-turn"),
        QString::fromUtf8("Fwd Corridor")  // index 5 — used by Full Exam
    };
    // DB keys — must match maneuver_type values stored in PARKING_SESSIONS / PARKING_MANEUVER_STEPS
    m_maneuverDbKeys = {"creneau", "bataille", "epi", "marche_arriere", "demi_tour", "marche_avant"};
    m_maneuverIcons = {
        QString::fromUtf8("🅿️"),
        QString::fromUtf8("🔲"),
        QString::fromUtf8("◆"),
        QString::fromUtf8("⬇️"),
        QString::fromUtf8("🔄"),
        QString::fromUtf8("\xe2\xac\x86\xef\xb8\x8f")  // ⬆️
    };
    m_maneuverColors = {"#00b894", "#0984e3", "#6c5ce7", "#e17055", "#fdcb6e", "#00cec9"};

    QList<ManeuverStep> creneau = {
        // ── Step 1: Safety checks & preparation ──
        {QString::fromUtf8("Safety checks"),
         QString::fromUtf8("Activate RIGHT indicator. Check all 3 mirrors + right blind spot."),
         QString::fromUtf8("BEFORE any movement:\n"
             "1. Interior mirror → rear traffic clear\n"
             "2. Right mirror → space alongside curb\n"
             "3. Right blind spot → turn your head!\n"
             "4. Right indicator ON to signal your intention\n"
             "\xe2\x9a\xa0 At the exam: forgetting visual checks = ELIMINATORY."),
         QString::fromUtf8("\xf0\x9f\x93\xa1 Sensors initializing... surroundings clear"), "PREP"},

        // ── Step 2: Alignment alongside the front car ──
        {QString::fromUtf8("Align alongside"),
         QString::fromUtf8("Drive forward, stop parallel to the front car. Keep 50-70 cm lateral distance."),
         QString::fromUtf8("KEY reference points:\n"
             "\xe2\x80\xa2 Your RIGHT MIRROR must align with the REAR BUMPER of the front car\n"
             "\xe2\x80\xa2 Lateral gap: 50-70 cm (about one arm's length)\n"
             "\xe2\x80\xa2 Too close (<40cm) → risk of scraping when steering\n"
             "\xe2\x80\xa2 Too far (>80cm) → not enough angle to enter the space\n"
             "\xf0\x9f\x92\xa1 Tip: use the right mirror to judge the gap precisely."),
         QString::fromUtf8("\xf0\x9f\x93\xa1 Right sensor: lateral distance OK (60cm)"), "ALIGN"},

        // ── Step 3: Full lock RIGHT + reverse slowly ──
        {QString::fromUtf8("Full lock RIGHT"),
         QString::fromUtf8("STOP. Steer full lock RIGHT while stationary, then reverse VERY slowly."),
         QString::fromUtf8("Execution sequence:\n"
             "1. Full stop (foot on brake)\n"
             "2. Turn steering wheel FULLY to the RIGHT (lock to lock)\n"
             "3. Clutch to biting point, release brake gently\n"
             "4. Reverse at WALKING SPEED — clutch control!\n"
             "\n"
             "Reference: watch the REAR-RIGHT corner of your car approaching the space.\n"
             "\xf0\x9f\x90\x8c Speed = control. Too fast = curb contact = eliminatory!"),
         QString::fromUtf8("\xf0\x9f\x94\x8a Right sensor: obstacle at 1.2m — continue slowly"), "TURN_RIGHT"},

        // ── Step 4: Check 45° angle — prepare counter-steer ──
        {QString::fromUtf8("Check 45\xc2\xb0 angle"),
         QString::fromUtf8("Your car is now at ~45\xc2\xb0. STOP when your right shoulder aligns with the front car's rear-left corner."),
         QString::fromUtf8("How to know you're at 45\xc2\xb0:\n"
             "\xe2\x80\xa2 Your right shoulder is level with the front car's corner\n"
             "\xe2\x80\xa2 In the LEFT mirror: you can start to see the curb appear\n"
             "\xe2\x80\xa2 In the RIGHT mirror: the rear car should NOT be visible yet\n"
             "\n"
             "\xe2\x9a\xa0 If you see the curb too close in the LEFT mirror → STOP immediately!\n"
             "This is the CRITICAL pivot point of the entire maneuver."),
         QString::fromUtf8("\xf0\x9f\x93\xb7 Left sensor triggered — 45\xc2\xb0 angle confirmed"), "CHECK_ANGLE"},

        // ── Step 5: Counter-steer full lock LEFT ──
        {QString::fromUtf8("Counter-steer LEFT"),
         QString::fromUtf8("STOP. Steer full lock LEFT while stationary, then continue reversing slowly."),
         QString::fromUtf8("Execution sequence:\n"
             "1. STOP completely (critical!)\n"
             "2. Turn steering wheel FULLY to the LEFT (lock to lock)\n"
             "3. Resume reversing very slowly with clutch control\n"
             "\n"
             "Mirror checks during this phase:\n"
             "\xe2\x80\xa2 LEFT mirror: watch curb distance — must stay >20cm\n"
             "\xe2\x80\xa2 RIGHT mirror: watch gap with rear car\n"
             "\xe2\x80\xa2 Interior mirror: check for traffic behind\n"
             "\xf0\x9f\x91\x80 Alternate between ALL 3 mirrors continuously!"),
         QString::fromUtf8("\xf0\x9f\x93\xb7 Rear sensors monitoring — curb visible in left mirror"), "TURN_LEFT"},

        // ── Step 6: Straighten & adjust to curb ──
        {QString::fromUtf8("Straighten wheels"),
         QString::fromUtf8("When parallel to curb: STOP, straighten wheels. Adjust forward/back to 20-30 cm from curb."),
         QString::fromUtf8("How to know you're parallel:\n"
             "\xe2\x80\xa2 Both mirrors show EQUAL distance to the curb\n"
             "\xe2\x80\xa2 The car body line follows the curb line\n"
             "\n"
             "Fine adjustment:\n"
             "\xe2\x80\xa2 Too far from curb → small RIGHT turn + reverse, then straighten\n"
             "\xe2\x80\xa2 Too close to curb → small LEFT turn + forward, then straighten\n"
             "\xe2\x80\xa2 Target: 20-30 cm from curb (exam requirement)\n"
             "\xe2\x9c\x85 At the exam: you must park in LESS than 3 minutes."),
         QString::fromUtf8("\xe2\x9c\x85 Both sensors OK — parallel, 25cm from curb"), "STRAIGHT"},

        // ── Step 7: Final position & securing ──
        {QString::fromUtf8("Secure vehicle"),
         QString::fromUtf8("Handbrake ON, gearbox in Neutral, turn OFF indicator, straighten wheels."),
         QString::fromUtf8("Final checklist:\n"
             "1. \xe2\x9c\x93 Handbrake firmly engaged\n"
             "2. \xe2\x9c\x93 Gear in Neutral (or P for automatic)\n"
             "3. \xe2\x9c\x93 Right indicator OFF\n"
             "4. \xe2\x9c\x93 Wheels perfectly straight\n"
             "5. \xe2\x9c\x93 Car centered in the space\n"
             "\n"
             "\xf0\x9f\x8f\x86 Exam criteria: parallel to curb, 20-30cm gap,\n"
             "   no obstacle contact, no curb mount, < 3 minutes."),
         QString::fromUtf8("\xf0\x9f\x8e\x89 Parallel parking complete — perfect execution!"), "DONE"}
    };
    QList<ManeuverStep> bataille = {
        {QString::fromUtf8("Spot the space"),
         QString::fromUtf8("Drive past the space by about 1 meter, keep 1.5m distance."),
         QString::fromUtf8("The further away, the more room you have.\n\xf0\x9f\x92\xa1 Use your mirror to see the space clearly."),
         QString::fromUtf8("\xf0\x9f\x93\xa1 Sensors: free space detected"), "DETECT"},
        {QString::fromUtf8("Full lock"),
         QString::fromUtf8("Reverse, steer toward the space, reverse slowly."),
         QString::fromUtf8("Reference: shoulder at the dividing line.\n\xe2\x9a\xa0 Look BOTH ways !"),
         QString::fromUtf8("\xf0\x9f\x94\x8a 80cm right \xc2\xb7 1.5m left"), "TURN"},
        {QString::fromUtf8("Center align"),
         QString::fromUtf8("Almost perpendicular — straighten wheels, keep reversing."),
         QString::fromUtf8("Center between lines or vehicles.\n\xf0\x9f\x91\x80 Check equal space on both sides."),
         QString::fromUtf8("\xf0\x9f\x93\xb7 Centering..."), "CENTER"},
        {QString::fromUtf8("Finalize"),
         QString::fromUtf8("Stop at the back of the space. Handbrake, neutral."),
         QString::fromUtf8("Don't go past the rear marking. No contact."),
         QString::fromUtf8("\xe2\x9c\x85 Position correct!"), "DONE"}
    };
    QList<ManeuverStep> epi = {
        {QString::fromUtf8("Approach"),
         QString::fromUtf8("Keep to the opposite side of the spaces (~1.5m). Locate the free space."),
         QString::fromUtf8("Angled spaces at 45\xc2\xb0 or 60\xc2\xb0.\n\xf0\x9f\x92\xa1 Identify the steering point."),
         QString::fromUtf8("\xf0\x9f\x93\xa1 Diagonal space detected"), "DETECT"},
        {QString::fromUtf8("Steering point"),
         QString::fromUtf8("Shoulder past the 1st line \xe2\x86\x92 steer toward the space."),
         QString::fromUtf8("The angle depends on the space inclination.\n\xf0\x9f\x90\x8c Advance slowly to control."),
         QString::fromUtf8("\xf0\x9f\x94\x8a Clear path, continue"), "TURN"},
        {QString::fromUtf8("Enter the space"),
         QString::fromUtf8("Move forward straightening the wheel to align in the space."),
         QString::fromUtf8("Center between the markings.\n\xf0\x9f\x91\x80 Watch the mirrors for distances."),
         QString::fromUtf8("\xf0\x9f\x93\xb7 Obstacle at 2m, slow down"), "ENTER"},
        {QString::fromUtf8("Final position"),
         QString::fromUtf8("Drive to the back without touching the curb. Handbrake."),
         QString::fromUtf8("20-30 cm from the bumper.\n\xf0\x9f\x8f\x86 Well aligned, centered, no contact."),
         QString::fromUtf8("\xe2\x9c\x85 Diagonal parking complete!"), "DONE"}
    };
    QList<ManeuverStep> marche = {
        {QString::fromUtf8("Preparation"),
         QString::fromUtf8("Check all mirrors and the rear blind spot."),
         QString::fromUtf8("Reverse, wheel straight.\n\xe2\x9a\xa0 Turn your head BEFORE reversing!"),
         QString::fromUtf8("\xf0\x9f\x93\xa1 Rear sensors active \xe2\x80\x94 clear"), "PREP"},
        {QString::fromUtf8("Reverse straight"),
         QString::fromUtf8("Reverse slowly in a straight line with the clutch."),
         QString::fromUtf8("Interior mirror + rear window.\n\xf0\x9f\x90\x8c Correct immediately if deviating."),
         QString::fromUtf8("\xf0\x9f\x93\xb7 Straight trajectory OK"), "REVERSE"},
        {QString::fromUtf8("Correct trajectory"),
         QString::fromUtf8("Small wheel turn to correct, then straighten."),
         QString::fromUtf8("Turn toward where the rear should go.\n\xf0\x9f\x92\xa1 Small corrections > one big late correction."),
         QString::fromUtf8("\xf0\x9f\x94\x8a Slight deviation \xe2\x80\x94 correct"), "CORRECT"},
        {QString::fromUtf8("Stop"),
         QString::fromUtf8("Stop at the destination. Handbrake, neutral."),
         QString::fromUtf8("Stayed in the lane, no stall.\n\xf0\x9f\x8f\x86 Straight line, < 2 min."),
         QString::fromUtf8("\xe2\x9c\x85 Perfect reverse!"), "DONE"}
    };
    m_allManeuvers = { creneau, bataille, epi, marche };

    // 3-point turn (maneuver clé examen Tunisie)
    QList<ManeuverStep> demitour = {
        {QString::fromUtf8("Final checks"),
         QString::fromUtf8("Check mirrors + blind spots. Position yourself on the right side of the road."),
         QString::fromUtf8("Left indicator BEFORE starting.\nThe more to the right you are, the more room to turn.\n\xe2\x9a\xa0 Prohibited on highway, expressway, one-way street."),
         QString::fromUtf8("\xf0\x9f\x93\xa1 Checking the surroundings..."), "PREP"},
        {QString::fromUtf8("Forward + steer left"),
         QString::fromUtf8("1st: advance slowly with full lock LEFT. Stop before the opposite curb."),
         QString::fromUtf8("Very slow with the clutch.\nCounter-steer 1 meter BEFORE the curb to prepare the reverse.\n\xf0\x9f\x92\xa1 The wheels will already be in the right direction."),
         QString::fromUtf8("\xf0\x9f\x94\x8a Opposite curb at 1m \xe2\x80\x94 STOP!"), "FORWARD"},
        {QString::fromUtf8("Reverse + steer right"),
         QString::fromUtf8("Reverse: full lock RIGHT. Reverse slowly toward the starting curb."),
         QString::fromUtf8("Turn around for direct rear vision.\nAlternate: mirrors + direct look.\nStop BEFORE the curb behind.\n\xe2\x9a\xa0 Pedestrian coming? STOP immediately."),
         QString::fromUtf8("\xf0\x9f\x93\xb7 Rear curb detected \xe2\x80\x94 stop"), "REVERSE"},
        {QString::fromUtf8("Drive forward"),
         QString::fromUtf8("1st gear, straighten the wheel and drive off in the new direction."),
         QString::fromUtf8("Straight wheels, check traffic before driving off.\nIf 3 points aren't enough, you can do 5.\n\xf0\x9f\x8f\x86 Goal: smooth, safe, no curb contact."),
         QString::fromUtf8("\xe2\x9c\x85 U-turn complete!"), "DONE"}
    };
    m_allManeuvers.append(demitour);

    // ── Marche avant dans un couloir étroit (index 5 — Full Exam only) ──
    QList<ManeuverStep> marcheAvant = {
        {QString::fromUtf8("Safety checks"),
         QString::fromUtf8("Check all mirrors + front blind areas. Left indicator ON."),
         QString::fromUtf8("Before entering the corridor:\n"
             "1. Interior mirror — is the corridor clear?\n"
             "2. Left mirror — distance to left barrier\n"
             "3. Right mirror — distance to right barrier\n"
             "4. Look ahead through the windscreen\n"
             "\xe2\x9a\xa0 The corridor is narrow (~2.8m). Stay CENTERED."),
         QString::fromUtf8("\xf0\x9f\x93\xa1 Sensors initializing — surroundings clear"), "PREP"},

        {QString::fromUtf8("Position at entry"),
         QString::fromUtf8("Align with the corridor centerline. Keep equal distance from both barriers."),
         QString::fromUtf8("Entry technique:\n"
             "\xe2\x80\xa2 Approach slowly in 1st gear with clutch control\n"
             "\xe2\x80\xa2 Center the car — equal gap to left and right barriers\n"
             "\xe2\x80\xa2 Target gap: >30 cm on each side\n"
             "\xf0\x9f\x92\xa1 If your left sensor reads <30cm, steer gently right."),
         QString::fromUtf8("\xf0\x9f\x93\xa1 Both side sensors: clear — entry aligned"), "ALIGN"},

        {QString::fromUtf8("Drive forward — entry"),
         QString::fromUtf8("Advance at walking speed. Monitor BOTH side sensors continuously."),
         QString::fromUtf8("Forward drive rules:\n"
             "\xe2\x80\xa2 Speed: 1st gear, clutch feathering — max 5 km/h\n"
             "\xe2\x80\xa2 Hands: relaxed, slight corrections only\n"
             "\xe2\x80\xa2 Eyes: windscreen ahead + alternate left/right mirrors\n"
             "\xe2\x80\xa2 If sensor beeps LEFT → steer 1/8 turn RIGHT, then straighten\n"
             "\xe2\x80\xa2 If sensor beeps RIGHT → steer 1/8 turn LEFT, then straighten\n"
             "\xf0\x9f\x90\x8c Small, frequent corrections > one big sudden movement"),
         QString::fromUtf8("\xf0\x9f\x94\x8a Left: 45cm  Right: 38cm — trajectory OK"), "DRIVE"},

        {QString::fromUtf8("Through corridor — stay centered"),
         QString::fromUtf8("Mid-corridor: maintain straight trajectory. STOP immediately if any sensor < 20cm."),
         QString::fromUtf8("Critical zone — halfway through:\n"
             "\xe2\x80\xa2 This is the narrowest perceived point — stay calm\n"
             "\xe2\x80\xa2 BOTH rear sensors now also active\n"
             "\xe2\x80\xa2 If front sensor < 40cm: SLOW DOWN further\n"
             "\xe2\x80\xa2 If front sensor < 20cm: STOP\n"
             "\xe2\x9a\xa0 At the ATTT exam: any contact = ELIMINATORY"),
         QString::fromUtf8("\xf0\x9f\x93\xb7 Mid-corridor — both sides clear"), "MIDWAY"},

        {QString::fromUtf8("Exit and secure"),
         QString::fromUtf8("Clear the exit line completely. Brake, handbrake, indicator off."),
         QString::fromUtf8("Exit sequence:\n"
             "1. Drive until your REAR bumper clears the far barrier line\n"
             "2. Brake smoothly and stop\n"
             "3. Handbrake engaged\n"
             "4. Gear in Neutral\n"
             "5. Left indicator OFF\n"
             "\xf0\x9f\x8f\x86 Exam criteria: no contact, centered trajectory, < 2 min."),
         QString::fromUtf8("\xe2\x9c\x85 Corridor cleared — forward drive complete!"), "DONE"}
    };
    m_allManeuvers.append(marcheAvant);
}

void ParkingWidget::initCommonMistakes()
{
    QList<CommonMistake> c0 = {
        {QString::fromUtf8("Skipping visual checks"), QString::fromUtf8("ELIMINATORY at exam"), QString::fromUtf8("3 mirrors + blind spot BEFORE moving")},
        {QString::fromUtf8("Positioning too far (>80cm)"), QString::fromUtf8("Not enough angle to enter space"), QString::fromUtf8("Keep 50-70 cm — one arm's length")},
        {QString::fromUtf8("Positioning too close (<40cm)"), QString::fromUtf8("Risk scraping front car while steering"), QString::fromUtf8("Check gap in right mirror")},
        {QString::fromUtf8("Steering too early/late"), QString::fromUtf8("Curb contact = eliminatory"), QString::fromUtf8("Reference: shoulder at front car corner")},
        {QString::fromUtf8("Not stopping at 45\xc2\xb0"), QString::fromUtf8("Overshooting the pivot point"), QString::fromUtf8("STOP, counter-steer stationary, then resume")},
        {QString::fromUtf8("Reversing too fast"), QString::fromUtf8("Loss of control, curb mount"), QString::fromUtf8("Clutch control — walking speed always")},
        {QString::fromUtf8("Not checking all 3 mirrors"), QString::fromUtf8("Eliminatory + safety risk"), QString::fromUtf8("Alternate left/right/interior continuously")},
        {QString::fromUtf8("Forgetting to secure vehicle"), QString::fromUtf8("Exam fault: missing handbrake"), QString::fromUtf8("Handbrake \xe2\x86\x92 Neutral \xe2\x86\x92 Indicator off \xe2\x86\x92 Wheels straight")},
    };
    QList<CommonMistake> c1 = {
        {QString::fromUtf8("Not enough overshoot"), QString::fromUtf8("No room to turn"), QString::fromUtf8("Overshoot by 1 meter")},
        {QString::fromUtf8("Steering too late"), QString::fromUtf8("Missed the line"), QString::fromUtf8("Turn at shoulder line")},
        {QString::fromUtf8("Not centered"), QString::fromUtf8("Badly positioned"), QString::fromUtf8("Equal space on both sides")},
        {QString::fromUtf8("Reversed too far"), QString::fromUtf8("Wall contact"), QString::fromUtf8("Sensors + reverse camera")},
    };
    QList<CommonMistake> c2 = {
        {QString::fromUtf8("Wrong angle"), QString::fromUtf8("Overshoot"), QString::fromUtf8("Check 45/60\xc2\xb0 beforehand")},
        {QString::fromUtf8("Imprecise steering"), QString::fromUtf8("Missed entry"), QString::fromUtf8("Very slowly to control")},
        {QString::fromUtf8("No straightening"), QString::fromUtf8("Parked diagonally"), QString::fromUtf8("Straighten as the nose enters")},
        {QString::fromUtf8("Too fast"), QString::fromUtf8("Curb contact"), QString::fromUtf8("Crawling in 1st gear")},
    };
    QList<CommonMistake> c3 = {
        {QString::fromUtf8("Did not check rear"), QString::fromUtf8("Eliminatory!"), QString::fromUtf8("ALWAYS turn your head")},
        {QString::fromUtf8("Too fast"), QString::fromUtf8("Loss of control"), QString::fromUtf8("Control with the clutch")},
        {QString::fromUtf8("Overcorrection"), QString::fromUtf8("Zigzag"), QString::fromUtf8("1/8 turn then straighten")},
        {QString::fromUtf8("Stall"), QString::fromUtf8("2 stalls = elimination"), QString::fromUtf8("Progressive clutch")},
    };
    m_allMistakes = { c0, c1, c2, c3 };

    // U-turn mistakes
    QList<CommonMistake> c4 = {
        {QString::fromUtf8("No visual check"), QString::fromUtf8("Eliminatory"), QString::fromUtf8("Always check mirrors")},
        {QString::fromUtf8("Mounting the curb"), QString::fromUtf8("Eliminatory: pedestrian danger"), QString::fromUtf8("Stop 1m BEFORE the edge")},
        {QString::fromUtf8("Forgetting to counter-steer"), QString::fromUtf8("Needs 5+ steps"), QString::fromUtf8("Counter-steer 1m before the curb")},
        {QString::fromUtf8("Too fast"), QString::fromUtf8("Loss of control"), QString::fromUtf8("Clutch = very slow pace")},
    };
    m_allMistakes.append(c4);

    // Forward corridor mistakes
    QList<CommonMistake> c5 = {
        {QString::fromUtf8("No pre-entry checks"), QString::fromUtf8("Miss obstacles — eliminatory"), QString::fromUtf8("Always mirrors + look ahead before entering")},
        {QString::fromUtf8("Entering off-center"), QString::fromUtf8("Immediate barrier contact"), QString::fromUtf8("Align on the centerline BEFORE entry")},
        {QString::fromUtf8("Too fast"), QString::fromUtf8("No time to correct — barrier hit"), QString::fromUtf8("1st gear + clutch feathering — walking speed only")},
        {QString::fromUtf8("Overcorrecting"), QString::fromUtf8("Zigzag — hits opposite barrier"), QString::fromUtf8("1/8 turn only, then straighten immediately")},
    };
    m_allMistakes.append(c5);
}

// ═══════════════════════════════════════════════════════════════════
//  StepDiagramWidget — Bird's-eye parking diagram
// ═══════════════════════════════════════════════════════════════════

StepDiagramWidget::StepDiagramWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(240);
    setStyleSheet("background:transparent;");
}

void StepDiagramWidget::setStep(int maneuver, int step, int totalSteps)
{
    m_maneuver = maneuver;
    m_step = step;
    m_totalSteps = totalSteps;
    update();
}

void StepDiagramWidget::drawCar(QPainter &p, double cx, double cy, double angle,
    double w, double h, const QColor &color, bool isMain)
{
    p.save();
    p.translate(cx, cy);
    p.rotate(angle);

    if (isMain) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0,0,0,18));
        p.drawRoundedRect(QRectF(-w/2+2, -h/2+2, w, h), 5, 5);
    }

    p.setBrush(color);
    p.setPen(QPen(color.darker(125), isMain ? 1.5 : 0.8));
    p.drawRoundedRect(QRectF(-w/2, -h/2, w, h), 5, 5);

    p.setPen(Qt::NoPen);
    p.setBrush(color.lighter(112));
    p.drawRoundedRect(QRectF(-w/2+4, -h/2+h*0.22, w-8, h*0.50), 3, 3);

    p.setBrush(QColor(170, 215, 255, isMain ? 210 : 100));
    p.drawRoundedRect(QRectF(-w/2+4, -h/2+3, w-8, h*0.17), 2, 2);

    p.setBrush(QColor(170, 215, 255, isMain ? 160 : 80));
    p.drawRoundedRect(QRectF(-w/2+4, h/2-h*0.18, w-8, h*0.15), 2, 2);

    if (isMain) {
        p.setBrush(QColor(255, 250, 180));
        p.drawRoundedRect(QRectF(-w/2+1, -h/2, 6, 4), 1, 1);
        p.drawRoundedRect(QRectF(w/2-7, -h/2, 6, 4), 1, 1);
        p.setBrush(QColor(255, 50, 50));
        p.drawRoundedRect(QRectF(-w/2+1, h/2-4, 6, 3), 1, 1);
        p.drawRoundedRect(QRectF(w/2-7, h/2-4, 6, 3), 1, 1);
        p.setBrush(QColor(35, 35, 35));
        double ww = 5, wh = 10;
        p.drawRoundedRect(QRectF(-w/2-3, -h/2+7, ww, wh), 1, 1);
        p.drawRoundedRect(QRectF(w/2-2, -h/2+7, ww, wh), 1, 1);
        p.drawRoundedRect(QRectF(-w/2-3, h/2-17, ww, wh), 1, 1);
        p.drawRoundedRect(QRectF(w/2-2, h/2-17, ww, wh), 1, 1);
        p.setBrush(QColor(255,255,255,180));
        QPolygonF tri;
        tri << QPointF(0, -h/2-5) << QPointF(-4, -h/2+1) << QPointF(4, -h/2+1);
        p.drawPolygon(tri);
    }
    p.restore();
}

void StepDiagramWidget::drawParkingSlot(QPainter &, double, double, double, double, bool) {}

void StepDiagramWidget::drawArrow(QPainter &p, double x1, double y1, double x2, double y2, const QColor &color)
{
    QPen pen(color, 2.5); pen.setStyle(Qt::DashLine); pen.setDashPattern({5, 3});
    p.setPen(pen);
    p.drawLine(QPointF(x1,y1), QPointF(x2,y2));
    double a = qAtan2(y2-y1, x2-x1); double sz = 10;
    QPointF tip(x2,y2), pa(x2-sz*qCos(a-0.35),y2-sz*qSin(a-0.35)), pb(x2-sz*qCos(a+0.35),y2-sz*qSin(a+0.35));
    p.setBrush(color); p.setPen(Qt::NoPen);
    p.drawPolygon(QPolygonF({tip, pa, pb}));
}

void StepDiagramWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    double W = width(), H = height();

    QLinearGradient bg(0,0,0,H);
    bg.setColorAt(0, QColor(246,249,253)); bg.setColorAt(1, QColor(238,243,248));
    p.setBrush(bg); p.setPen(QPen(QColor(220,228,238),1));
    p.drawRoundedRect(QRect(0,0,W,H), 12, 12);

    double cW = 30, cH = 56;
    QColor myCol(0,184,148), ghostCol(0,184,148,35), parkedCol(155,165,178);

    if (m_totalSteps == 0 || m_maneuver < 0) {
        p.setPen(QColor(180,190,200)); p.setFont(QFont("Segoe UI",11));
        p.drawText(rect(), Qt::AlignCenter, "Select a maneuver");
        return;
    }

    // ════════════════════════════════════════════════════════════
    //  PARALLEL PARKING (Créneau) — 7-step professional diagram
    //  Bird's-eye view: cars point LEFT (angle=-90 means nose left)
    // ════════════════════════════════════════════════════════════
    if (m_maneuver == 0) {
        double roadTop = H*0.04, roadBot = H*0.44;

        // ── Road surface ──
        p.setPen(Qt::NoPen); p.setBrush(QColor(210,215,225));
        p.drawRect(QRectF(0, roadTop, W, roadBot-roadTop));
        // Center dashes
        QPen dl(QColor(255,255,255,180),2,Qt::CustomDashLine); dl.setDashPattern({8,10});
        p.setPen(dl); p.drawLine(QPointF(0,(roadTop+roadBot)/2), QPointF(W,(roadTop+roadBot)/2));

        // ── Direction arrows on road ──
        p.setPen(QPen(QColor(180,185,195),1.2));
        for (int ax = 40; ax < (int)W-40; ax += 70) {
            double ay = roadTop+10;
            p.drawLine(QPointF(ax+8,ay), QPointF(ax,ay));
            p.drawLine(QPointF(ax+3,ay-3), QPointF(ax,ay));
            p.drawLine(QPointF(ax+3,ay+3), QPointF(ax,ay));
        }

        // ── Curb ──
        QLinearGradient cg(0,roadBot,0,roadBot+10);
        cg.setColorAt(0,QColor(165,170,178)); cg.setColorAt(0.5,QColor(150,155,163)); cg.setColorAt(1,QColor(140,145,153));
        p.setPen(Qt::NoPen); p.setBrush(cg);
        p.drawRect(QRectF(0,roadBot,W,10));
        // Sidewalk
        p.setBrush(QColor(232,236,240)); p.drawRect(QRectF(0,roadBot+10,W,H-roadBot-10));
        // Sidewalk texture lines
        p.setPen(QPen(QColor(218,222,228),0.5));
        for(double tx = 20; tx < W; tx += 35)
            p.drawLine(QPointF(tx,roadBot+12), QPointF(tx,H-4));

        double parkMidY = roadBot + 10 + cW/2 + 12;
        double frontX = W*0.68, rearX = W*0.20;

        // ── Parked cars (angle -90 = pointing left) ──
        drawCar(p, frontX, parkMidY, -90, cW, cH, parkedCol, false);
        drawCar(p, rearX, parkMidY, -90, cW, cH, parkedCol, false);

        // Labels for parked cars
        p.setFont(QFont("Segoe UI",7,QFont::Bold)); p.setPen(QColor(120,130,145));
        p.drawText(QRectF(frontX-cH/2, parkMidY-cW/2-14, cH, 12), Qt::AlignCenter, "Vehicle A (front)");
        p.drawText(QRectF(rearX-cH/2, parkMidY-cW/2-14, cH, 12), Qt::AlignCenter, "Vehicle B (rear)");

        // ── Parking space ──
        double spL = rearX + cH/2 + 6, spR = frontX - cH/2 - 6, spMx = (spL+spR)/2;
        p.setBrush(QColor(0,184,148,10));
        p.setPen(QPen(QColor(0,184,148,80),1.5,Qt::DashDotLine));
        p.drawRoundedRect(QRectF(spL, parkMidY-cW/2-8, spR-spL, cW+16), 4, 4);
        p.setFont(QFont("Segoe UI",16,QFont::Bold)); p.setPen(QColor(0,184,148,25));
        p.drawText(QRectF(spL, parkMidY-cW/2-8, spR-spL, cW+16), Qt::AlignCenter, "P");

        // ── Road center Y for car positions on road ──
        double rMidY = (roadTop+roadBot)/2 + 6;

        // ── 7-step car positions ──
        // angles: -90=pointing left, more negative=nose turns down(into space)
        struct Pos { double x,y,a; };
        Pos steps[] = {
            {frontX + cH*0.6, rMidY,       -90},    // 0: PREP — stopped before front car
            {frontX + 4,      rMidY,       -90},    // 1: ALIGN — mirror-to-bumper aligned
            {frontX - cH*0.3, rMidY+14,    -60},    // 2: TURN_RIGHT — reversing, rear swings toward curb
            {spMx + 18,       rMidY+22,    -45},    // 3: CHECK_ANGLE — 45° key moment
            {spMx + 10,       roadBot+4,   -105},   // 4: TURN_LEFT — counter-steering, nose swings in
            {spMx + 2,        parkMidY-3,  -92},    // 5: STRAIGHT — almost parallel
            {spMx,            parkMidY,    -90}      // 6: DONE — perfectly parked
        };

        // ── Ghost trail of past steps ──
        for (int i = 0; i < m_step && i < 7; i++)
            drawCar(p, steps[i].x, steps[i].y, steps[i].a, cW, cH, ghostCol, false);

        // ── Trajectory curve ──
        if (m_step > 0) {
            QPainterPath path; path.moveTo(steps[0].x, steps[0].y);
            for (int i = 1; i <= qMin(m_step,6); i++)
                path.quadTo((steps[i-1].x+steps[i].x)/2, (steps[i-1].y+steps[i].y)/2+10,
                            steps[i].x, steps[i].y);
            p.setPen(QPen(QColor(0,184,148,50),2.2,Qt::DotLine)); p.setBrush(Qt::NoBrush);
            p.drawPath(path);
        }

        // ── Active car ──
        int s = qMin(m_step, 6);
        drawCar(p, steps[s].x, steps[s].y, steps[s].a, cW, cH, myCol, true);

        // ── Next-step arrow ──
        if (m_step < 6)
            drawArrow(p, steps[s].x-10, steps[s].y+6, steps[s+1].x+10, steps[s+1].y-6, QColor(0,184,148,120));

        // ════════════════════════════════════════════
        //  Step-by-step ANNOTATIONS (the key pedagogy)
        // ════════════════════════════════════════════
        QColor annRed(225,112,85), annBlue(9,132,227), annPurple(108,92,231);
        QColor annGreen(0,184,148), annYellow(253,203,110), annOrange(230,126,34);
        p.setFont(QFont("Segoe UI",8,QFont::Bold));

        if (m_step == 0) {
            // ── PREP: Safety checks ──
            // Eye icon at each mirror position
            p.setFont(QFont("Segoe UI",11)); p.setPen(annBlue);
            p.drawText(QPointF(steps[0].x-6, steps[0].y-cW/2-4), QString::fromUtf8("\xf0\x9f\x91\x81"));
            // Blind spot arc
            p.setPen(QPen(annOrange,2,Qt::DashLine)); p.setBrush(QColor(230,126,34,20));
            p.drawPie(QRectF(steps[0].x-30, steps[0].y-30, 60, 60), -20*16, -50*16);
            p.setFont(QFont("Segoe UI",7,QFont::Bold)); p.setPen(annOrange);
            p.drawText(QPointF(steps[0].x+18, steps[0].y+26), "Blind spot");
            // Indicator arrow
            p.setPen(QPen(annYellow,2.5)); p.setBrush(annYellow);
            QPolygonF ind; ind << QPointF(steps[0].x+cH/2+4, steps[0].y+cW/2-2)
                               << QPointF(steps[0].x+cH/2+12, steps[0].y+cW/2+4)
                               << QPointF(steps[0].x+cH/2+4, steps[0].y+cW/2+4);
            p.drawPolygon(ind);
            p.setFont(QFont("Segoe UI",8,QFont::Bold)); p.setPen(annBlue);
            p.drawText(QPointF(W*0.02, roadTop+14), QString::fromUtf8("\xf0\x9f\x91\x81 Check 3 mirrors + blind spot"));
            p.setPen(annYellow.darker(120));
            p.drawText(QPointF(W*0.02, roadTop+28), QString::fromUtf8("\xe2\x9e\xa1 RIGHT indicator ON"));

        } else if (m_step == 1) {
            // ── ALIGN: Alongside front car ──
            // Lateral distance measurement (car to parked car)
            double carRight = steps[1].y + cW/2;
            double parkedTop = parkMidY - cW/2;
            p.setPen(QPen(annRed,1.8,Qt::DashLine));
            p.drawLine(QPointF(steps[1].x-8, carRight+2), QPointF(steps[1].x-8, roadBot-1));
            // Measurement ticks
            p.drawLine(QPointF(steps[1].x-14, carRight+2), QPointF(steps[1].x-2, carRight+2));
            p.drawLine(QPointF(steps[1].x-14, roadBot-1), QPointF(steps[1].x-2, roadBot-1));
            p.setPen(annRed);
            p.drawText(QPointF(steps[1].x+4, (carRight+roadBot)/2+4), "50-70 cm");

            // Mirror-to-bumper alignment line
            p.setPen(QPen(annPurple,1.5,Qt::DotLine));
            double mirrorY = steps[1].y + cW/2 - 4;
            double bumperX = frontX + cH/2;
            p.drawLine(QPointF(steps[1].x, mirrorY), QPointF(bumperX, parkMidY-cW/2+2));
            // Circle on mirror position
            p.setPen(QPen(annPurple,2)); p.setBrush(QColor(108,92,231,40));
            p.drawEllipse(QPointF(steps[1].x, mirrorY), 5, 5);
            p.setPen(annPurple); p.setFont(QFont("Segoe UI",7,QFont::Bold));
            p.drawText(QPointF(steps[1].x-cH/2-4, mirrorY+3), "Mirror");
            p.setFont(QFont("Segoe UI",8,QFont::Bold)); p.setPen(annPurple);
            p.drawText(QPointF(W*0.02, roadTop+14), QString::fromUtf8("\xf0\x9f\x93\x8f Mirror \xe2\x87\x94 rear bumper"));
            p.setPen(annRed);
            p.drawText(QPointF(W*0.02, roadTop+28), "50-70 cm lateral gap");

        } else if (m_step == 2) {
            // ── TURN_RIGHT: Full lock right + reverse ──
            // Steering wheel indicator
            p.setPen(QPen(annYellow,3)); p.setBrush(Qt::NoBrush);
            double swX = W*0.88, swY = H*0.82, swR = 16;
            p.drawEllipse(QPointF(swX, swY), swR, swR);
            // Steering direction arrow (clockwise = right)
            p.setPen(QPen(annRed,2.5));
            p.drawArc(QRectF(swX-swR+4, swY-swR+4, (swR-4)*2, (swR-4)*2), 90*16, -180*16);
            QPolygonF sa; sa << QPointF(swX+swR-6, swY+2) << QPointF(swX+swR-2, swY-4) << QPointF(swX+swR-1, swY+6);
            p.setBrush(annRed); p.setPen(Qt::NoPen); p.drawPolygon(sa);
            p.setFont(QFont("Segoe UI",6,QFont::Bold)); p.setPen(annRed);
            p.drawText(QRectF(swX-swR-2, swY+swR+2, swR*2+4, 14), Qt::AlignCenter, "FULL RIGHT");

            // Arc showing turn radius from car
            p.setPen(QPen(annYellow,2.5)); p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(steps[2].x-32, steps[2].y-32, 64, 64), (int)(-steps[2].a)*16, 30*16);
            p.setPen(annYellow); p.setFont(QFont("Segoe UI",7,QFont::Bold));
            p.drawText(QPointF(steps[2].x+36, steps[2].y-14), QString::fromUtf8("~35\xc2\xb0"));

            // Reverse direction arrow
            p.setPen(QPen(annBlue,2)); p.setBrush(Qt::NoBrush);
            double arrX = steps[2].x + cH/2 + 6, arrY = steps[2].y;
            p.drawLine(QPointF(arrX, arrY-12), QPointF(arrX, arrY+12));
            QPolygonF ra; ra << QPointF(arrX, arrY+14) << QPointF(arrX-4, arrY+8) << QPointF(arrX+4, arrY+8);
            p.setBrush(annBlue); p.setPen(Qt::NoPen); p.drawPolygon(ra);
            p.setFont(QFont("Segoe UI",8,QFont::Bold)); p.setPen(annRed);
            p.drawText(QPointF(W*0.02, roadTop+14), QString::fromUtf8("\xf0\x9f\x94\x84 FULL RIGHT \xe2\x80\x94 Reverse slowly"));

        } else if (m_step == 3) {
            // ── CHECK_ANGLE: 45° pivot point ──
            // 45° angle arc
            p.setPen(QPen(annOrange,2.5)); p.setBrush(QColor(230,126,34,25));
            p.drawPie(QRectF(steps[3].x-36, steps[3].y-36, 72, 72), 90*16, -45*16);
            p.setPen(annOrange); p.setFont(QFont("Segoe UI",9,QFont::Bold));
            p.drawText(QPointF(steps[3].x+12, steps[3].y-30), QString::fromUtf8("45\xc2\xb0"));

            // Shoulder reference line to front car corner
            p.setPen(QPen(annPurple,1.5,Qt::DashDotLine));
            double shoulderX = steps[3].x - cH*0.15;
            double shoulderY = steps[3].y - cW*0.3;
            double cornerX = frontX - cH/2;
            double cornerY = parkMidY + cW/2;
            p.drawLine(QPointF(shoulderX, shoulderY), QPointF(cornerX, cornerY));
            // Mark shoulder point
            p.setPen(QPen(annPurple,2)); p.setBrush(QColor(108,92,231,60));
            p.drawEllipse(QPointF(shoulderX, shoulderY), 4, 4);
            p.setFont(QFont("Segoe UI",6,QFont::Bold)); p.setPen(annPurple);
            p.drawText(QPointF(shoulderX-32, shoulderY-6), "Shoulder");

            // STOP sign
            p.setFont(QFont("Segoe UI",9,QFont::Bold)); p.setPen(Qt::NoPen);
            p.setBrush(QColor(214,48,49,200));
            p.drawRoundedRect(QRectF(W*0.82, H*0.78, 52, 22), 4, 4);
            p.setPen(Qt::white); p.drawText(QRectF(W*0.82, H*0.78, 52, 22), Qt::AlignCenter, "STOP");
            p.setFont(QFont("Segoe UI",8,QFont::Bold)); p.setPen(annOrange);
            p.drawText(QPointF(W*0.02, roadTop+14), QString::fromUtf8("\xe2\x9a\xa0 45\xc2\xb0 CRITICAL \xe2\x80\x94 STOP!"));

        } else if (m_step == 4) {
            // ── TURN_LEFT: Counter-steer ──
            // Steering wheel indicator (LEFT)
            p.setPen(QPen(annYellow,3)); p.setBrush(Qt::NoBrush);
            double swX = W*0.88, swY = H*0.82, swR = 16;
            p.drawEllipse(QPointF(swX, swY), swR, swR);
            p.setPen(QPen(annBlue,2.5));
            p.drawArc(QRectF(swX-swR+4, swY-swR+4, (swR-4)*2, (swR-4)*2), 90*16, 180*16);
            QPolygonF sa; sa << QPointF(swX-swR+6, swY+2) << QPointF(swX-swR+2, swY-4) << QPointF(swX-swR+1, swY+6);
            p.setBrush(annBlue); p.setPen(Qt::NoPen); p.drawPolygon(sa);
            p.setFont(QFont("Segoe UI",6,QFont::Bold)); p.setPen(annBlue);
            p.drawText(QRectF(swX-swR-2, swY+swR+2, swR*2+4, 14), Qt::AlignCenter, "FULL LEFT");

            // Mirror check indicators — 3 eyes
            p.setFont(QFont("Segoe UI",10));
            p.setPen(annBlue);
            // Left mirror eye
            p.drawText(QPointF(steps[4].x+cH/2+4, steps[4].y+cW/2+2), QString::fromUtf8("\xf0\x9f\x91\x81"));
            // Right mirror eye
            p.drawText(QPointF(steps[4].x-cH/2-16, steps[4].y-cW/2-2), QString::fromUtf8("\xf0\x9f\x91\x81"));
            // Interior mirror eye
            p.drawText(QPointF(steps[4].x-4, steps[4].y-cW/2-14), QString::fromUtf8("\xf0\x9f\x91\x81"));

            // Curb watch line from left mirror
            p.setPen(QPen(annRed,1.5,Qt::DashLine));
            p.drawLine(QPointF(steps[4].x+cH/2+2, steps[4].y+cW/2), QPointF(steps[4].x+cH/2+20, roadBot+4));
            p.setFont(QFont("Segoe UI",6,QFont::Bold)); p.setPen(annRed);
            p.drawText(QPointF(steps[4].x+cH/2+22, roadBot+8), "> 20cm!");
            p.setFont(QFont("Segoe UI",8,QFont::Bold)); p.setPen(annBlue);
            p.drawText(QPointF(W*0.02, roadTop+14), QString::fromUtf8("\xf0\x9f\x94\x84 FULL LEFT \xe2\x80\x94 Watch mirrors"));

        } else if (m_step == 5) {
            // ── STRAIGHT: Straighten wheels ──
            // Distance to curb measurement
            double carBottom = steps[5].y + cW/2;
            p.setPen(QPen(annGreen,1.8,Qt::DashLine));
            p.drawLine(QPointF(steps[5].x-10, carBottom+2), QPointF(steps[5].x-10, roadBot-1));
            p.drawLine(QPointF(steps[5].x-16, carBottom+2), QPointF(steps[5].x-4, carBottom+2));
            p.drawLine(QPointF(steps[5].x-16, roadBot-1), QPointF(steps[5].x-4, roadBot-1));
            p.setPen(annGreen); p.setFont(QFont("Segoe UI",8,QFont::Bold));
            p.drawText(QPointF(steps[5].x+4, (carBottom+roadBot)/2+4), "20-30 cm");

            // Both mirrors check
            p.setFont(QFont("Segoe UI",7,QFont::Bold)); p.setPen(annPurple);
            p.drawText(QPointF(steps[5].x+cH/2+6, steps[5].y), "R mirror");
            p.drawText(QPointF(steps[5].x-cH/2-36, steps[5].y), "L mirror");
            // Equal sign between
            p.setPen(QPen(annGreen,1.5));
            p.drawLine(QPointF(steps[5].x+cH/2+4, steps[5].y+cW/2), QPointF(steps[5].x+cH/2+4, roadBot));
            p.drawLine(QPointF(steps[5].x-cH/2-4, steps[5].y+cW/2), QPointF(steps[5].x-cH/2-4, roadBot));

            // Steering wheel straight
            double swX = W*0.88, swY = H*0.82, swR = 14;
            p.setPen(QPen(annGreen,3)); p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(swX, swY), swR, swR);
            p.drawLine(QPointF(swX, swY-swR+3), QPointF(swX, swY+swR-3));
            p.setFont(QFont("Segoe UI",6,QFont::Bold)); p.setPen(annGreen);
            p.drawText(QRectF(swX-swR-2, swY+swR+2, swR*2+4, 14), Qt::AlignCenter, "STRAIGHT");

        } else if (m_step == 6) {
            // ── DONE: Final secured position ──
            // Success checkmark
            p.setFont(QFont("Segoe UI",18)); p.setPen(annGreen);
            p.drawText(QPointF(steps[6].x-cH/2-22, steps[6].y+8), QString::fromUtf8("\xe2\x9c\x85"));

            // Final distance to curb
            double carBottom = steps[6].y + cW/2;
            p.setPen(QPen(annGreen,2));
            p.drawLine(QPointF(steps[6].x-10, carBottom+2), QPointF(steps[6].x-10, roadBot-1));
            p.setPen(annGreen); p.setFont(QFont("Segoe UI",7,QFont::Bold));
            p.drawText(QPointF(steps[6].x+2, (carBottom+roadBot)/2+4), "25 cm");

            // Handbrake icon
            double hbX = W*0.85, hbY = H*0.75;
            p.setPen(Qt::NoPen); p.setBrush(QColor(0,184,148,30));
            p.drawRoundedRect(QRectF(hbX-4, hbY-4, 62, 50), 6, 6);
            p.setFont(QFont("Segoe UI",7,QFont::Bold)); p.setPen(annGreen);
            p.drawText(QPointF(hbX, hbY+8),  QString::fromUtf8("\xe2\x9c\x93 Handbrake"));
            p.drawText(QPointF(hbX, hbY+20), QString::fromUtf8("\xe2\x9c\x93 Neutral"));
            p.drawText(QPointF(hbX, hbY+32), QString::fromUtf8("\xe2\x9c\x93 Indicator OFF"));
            p.drawText(QPointF(hbX, hbY+44), QString::fromUtf8("\xe2\x9c\x93 Wheels straight"));
        }

    // ════════════════════════════════════════════════════════════
    //  PERPENDICULAR (Bataille) — Annotated 4-step diagram
    // ════════════════════════════════════════════════════════════
    } else if (m_maneuver == 1) {
        double curbY = H*0.48;
        // Road
        p.setPen(Qt::NoPen); p.setBrush(QColor(210,215,225)); p.drawRect(QRectF(0,0,W,curbY));
        // Curb
        QLinearGradient cg1(0,curbY,0,curbY+8);
        cg1.setColorAt(0,QColor(165,170,178)); cg1.setColorAt(1,QColor(145,150,158));
        p.setBrush(cg1); p.drawRect(QRectF(0,curbY,W,8));
        // Parking area
        p.setBrush(QColor(232,236,240)); p.drawRect(QRectF(0,curbY+8,W,H-curbY-8));

        // Parking slots
        double slotMx = W*0.45, slotY = curbY+12, sw2 = cW+14;
        for (int i = -1; i <= 1; i++) {
            double sx = slotMx + i*(sw2+8);
            bool occ = (i!=0);
            p.setBrush(occ?QColor(228,230,234):QColor(0,184,148,12));
            p.setPen(QPen(occ?QColor(185,192,200):QColor(0,184,148,90),1.5,Qt::DashLine));
            p.drawRect(QRectF(sx-sw2/2,slotY,sw2,cH+12));
            if(occ) drawCar(p,sx,slotY+(cH+12)/2,0,cW,cH,parkedCol,false);
        }
        // Label occupied slots
        p.setFont(QFont("Segoe UI",6,QFont::Bold)); p.setPen(QColor(140,150,165));
        p.drawText(QRectF(slotMx-(sw2+8)-sw2/2, slotY-12, sw2, 11), Qt::AlignCenter, "Occupied");
        p.drawText(QRectF(slotMx+(sw2+8)-sw2/2, slotY-12, sw2, 11), Qt::AlignCenter, "Occupied");
        // Free space label
        p.setPen(QColor(0,184,148)); p.setFont(QFont("Segoe UI",14,QFont::Bold));
        p.drawText(QRectF(slotMx-sw2/2, slotY, sw2, cH+12), Qt::AlignCenter, "P");

        // Car positions
        struct Pos { double x,y,a; };
        double slotCy = slotY+(cH+12)/2;
        Pos st[] = {
            {W*0.78, curbY*0.42, 90},          // 0: driving past, 1.5m away
            {W*0.60, curbY*0.52, 50},           // 1: start turning into space
            {slotMx+6, curbY*0.78, 12},         // 2: nearly perpendicular
            {slotMx, slotCy, 0}                 // 3: centered in space
        };
        for(int i=0;i<m_step&&i<4;i++) drawCar(p,st[i].x,st[i].y,st[i].a,cW,cH,ghostCol,false);
        int s=qMin(m_step,3); drawCar(p,st[s].x,st[s].y,st[s].a,cW,cH,myCol,true);
        if(m_step<3) drawArrow(p,st[s].x,st[s].y+cH/2+4,st[s+1].x,st[s+1].y-cH/2-4,QColor(0,184,148,130));

        // ── Step annotations ──
        QColor annBlue(9,132,227), annRed(225,112,85), annGreen(0,184,148), annOrange(230,126,34), annPurple(108,92,231);
        p.setFont(QFont("Segoe UI",8,QFont::Bold));
        if(m_step == 0) {
            // Instruction text
            p.setPen(annBlue);
            p.drawText(QPointF(W*0.02, curbY*0.18), QString::fromUtf8("\xf0\x9f\x91\x81 Check mirrors + signal RIGHT"));
            // Overshoot distance annotation
            p.setPen(QPen(annRed,1.5,Qt::DashLine));
            p.drawLine(QPointF(slotMx, curbY*0.35), QPointF(st[0].x, curbY*0.35));
            p.setPen(annRed);
            p.drawText(QPointF((slotMx+st[0].x)/2-20, curbY*0.35-4), "~1m overshoot");
            // Lateral distance
            p.setPen(QPen(annBlue,1.5,Qt::DashLine));
            p.drawLine(QPointF(st[0].x+cW/2+2, st[0].y-10), QPointF(st[0].x+cW/2+2, curbY-2));
            p.setPen(annBlue);
            p.drawText(QPointF(st[0].x+cW/2+6, (st[0].y+curbY)/2), "1.5m");
            // Blind spot arcs
            p.setPen(QPen(annOrange,1.5,Qt::DashLine)); p.setBrush(QColor(230,126,34,15));
            p.drawPie(QRectF(st[0].x-20,st[0].y-20,40,40), -120*16, -40*16);
        } else if(m_step == 1) {
            // Instruction text
            p.setPen(annPurple);
            p.drawText(QPointF(W*0.02, curbY*0.18), QString::fromUtf8("\xf0\x9f\x94\x84 FULL RIGHT \xe2\x80\x94 Turn into space"));
            // Turning arc
            p.setPen(QPen(QColor(253,203,110),2.5)); p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(st[1].x-30,st[1].y-30,60,60), (int)(-st[1].a)*16, 40*16);
            // Steering wheel indicator
            double swX = W*0.88, swY = curbY*0.25, swR = 14;
            p.setPen(QPen(annPurple,3)); p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(swX, swY), swR, swR);
            p.setPen(QPen(annPurple,2.5));
            p.drawLine(QPointF(swX-swR+3, swY-4), QPointF(swX+swR-3, swY+4));
            p.setFont(QFont("Segoe UI",6,QFont::Bold)); p.setPen(annPurple);
            p.drawText(QRectF(swX-swR-2, swY+swR+2, swR*2+4, 14), Qt::AlignCenter, "FULL R");
        } else if(m_step == 2) {
            // Instruction text
            p.setPen(annGreen);
            p.drawText(QPointF(W*0.02, curbY*0.18), QString::fromUtf8("\xe2\x9a\x96 Straighten \xe2\x80\x94 Equal gaps both sides"));
            // Equal spacing lines
            double leftCar = slotMx-(sw2+8), rightCar = slotMx+(sw2+8);
            p.setPen(QPen(annGreen,1.2,Qt::DashLine));
            p.drawLine(QPointF(st[2].x-cW/2, st[2].y), QPointF(leftCar+cW/2+4, st[2].y));
            p.drawLine(QPointF(st[2].x+cW/2, st[2].y), QPointF(rightCar-cW/2-4, st[2].y));
            // Mirror labels
            p.setFont(QFont("Segoe UI",7,QFont::Bold)); p.setPen(annPurple);
            p.drawText(QPointF(st[2].x+cW/2+4, st[2].y-8), "R mirror");
            p.drawText(QPointF(st[2].x-cW/2-36, st[2].y-8), "L mirror");
        } else if(m_step == 3) {
            // Instruction text
            p.setPen(annGreen);
            p.drawText(QPointF(W*0.02, curbY*0.18), QString::fromUtf8("\xe2\x9c\x85 Centered \xe2\x80\x94 Handbrake + Neutral"));
            // Success + stop line
            p.setPen(QPen(annRed,2)); p.drawLine(QPointF(slotMx-sw2/2+4, slotY+cH+8), QPointF(slotMx+sw2/2-4, slotY+cH+8));
            p.setFont(QFont("Segoe UI",6,QFont::Bold)); p.setPen(annRed);
            p.drawText(QPointF(slotMx+sw2/2+2, slotY+cH+10), "STOP line");
            p.setFont(QFont("Segoe UI",14)); p.setPen(annGreen);
            p.drawText(QPointF(st[3].x-cW/2-20, st[3].y+6), QString::fromUtf8("\xe2\x9c\x85"));
            // Checklist
            double hbX = W*0.02, hbY = slotY+cH+22;
            p.setFont(QFont("Segoe UI",7,QFont::Bold)); p.setPen(annGreen);
            p.drawText(QPointF(hbX, hbY+10), QString::fromUtf8("\xe2\x9c\x93 Handbrake"));
            p.drawText(QPointF(hbX, hbY+22), QString::fromUtf8("\xe2\x9c\x93 Neutral"));
            p.drawText(QPointF(hbX, hbY+34), QString::fromUtf8("\xe2\x9c\x93 Indicator OFF"));
        }

    // ════════════════════════════════════════════════════════════
    //  DIAGONAL (Épi) — Annotated 4-step diagram
    // ════════════════════════════════════════════════════════════
    } else if (m_maneuver == 2) {
        double curbY = H*0.48;
        p.setPen(Qt::NoPen); p.setBrush(QColor(210,215,225)); p.drawRect(QRectF(0,0,W,curbY));
        QLinearGradient cg2(0,curbY,0,curbY+8);
        cg2.setColorAt(0,QColor(165,170,178)); cg2.setColorAt(1,QColor(145,150,158));
        p.setBrush(cg2); p.drawRect(QRectF(0,curbY,W,8));
        p.setBrush(QColor(232,236,240)); p.drawRect(QRectF(0,curbY+8,W,H-curbY-8));

        // Diagonal parking spaces (angled at ~50°)
        double diagAngle = -50;
        for(int i=0;i<3;i++){
            double sx=W*0.25+i*(cW+22); p.save(); p.translate(sx,curbY+12); p.rotate(diagAngle);
            p.setBrush(i==1?QColor(0,184,148,12):QColor(228,230,234));
            p.setPen(QPen(i==1?QColor(0,184,148,90):QColor(180,190,200),1.5,Qt::DashLine));
            p.drawRect(QRectF(0,0,cW+8,cH+8));
            if(i!=1){p.setBrush(parkedCol);p.setPen(QPen(parkedCol.darker(120),1));
                p.drawRoundedRect(QRectF(4,4,cW,cH),3,3);}
            p.restore();
        }
        // Angle annotation
        p.setPen(QPen(QColor(108,92,231),1.5)); p.setBrush(Qt::NoBrush);
        double angX = W*0.25+(cW+22), angY = curbY+12;
        p.drawArc(QRectF(angX-18,angY-18,36,36), 0, (int)(-diagAngle)*16);
        p.setFont(QFont("Segoe UI",7,QFont::Bold)); p.setPen(QColor(108,92,231));
        p.drawText(QPointF(angX+20, angY+4), QString::fromUtf8("50\xc2\xb0"));

        struct Pos { double x,y,a; };
        Pos st[] = {
            {W*0.78, curbY*0.32, 90},       // 0: approach from opposite side
            {W*0.58, curbY*0.48, 55},        // 1: steering point reached
            {W*0.46, curbY*0.70, 40},        // 2: entering space
            {W*0.38, curbY*0.88, 40}         // 3: final position
        };
        for(int i=0;i<m_step&&i<4;i++) drawCar(p,st[i].x,st[i].y,st[i].a,cW,cH,ghostCol,false);
        int s=qMin(m_step,3); drawCar(p,st[s].x,st[s].y,st[s].a,cW,cH,myCol,true);
        if(m_step<3) drawArrow(p,st[s].x-6,st[s].y+cH/2+2,st[s+1].x+6,st[s+1].y-cH/2-2,QColor(0,184,148,130));

        // ── Step annotations ──
        QColor annBlue(9,132,227), annGreen(0,184,148), annOrange(230,126,34), annPurple(108,92,231);
        p.setFont(QFont("Segoe UI",8,QFont::Bold));
        if(m_step == 0) {
            // Instruction text
            p.setPen(annBlue);
            p.drawText(QPointF(W*0.02, curbY*0.18), QString::fromUtf8("\xf0\x9f\x91\x81 Signal RIGHT \xe2\x80\x94 Approach at 1.5m"));
            // Distance line
            p.setPen(QPen(annBlue,1.5,Qt::DashLine));
            p.drawLine(QPointF(st[0].x+cW/2+2, st[0].y), QPointF(st[0].x+cW/2+2, curbY-2));
            p.setPen(annBlue);
            p.drawText(QPointF(st[0].x+cW/2+6, (st[0].y+curbY)/2), "~1.5m");
            // Blind spot arc
            p.setPen(QPen(annOrange,1.5,Qt::DashLine)); p.setBrush(QColor(230,126,34,15));
            p.drawPie(QRectF(st[0].x-20,st[0].y-20,40,40), -120*16, -40*16);
        } else if(m_step == 1) {
            // Instruction text
            p.setPen(annPurple);
            p.drawText(QPointF(W*0.02, curbY*0.18), QString::fromUtf8("\xf0\x9f\x94\x84 Steer into angle \xe2\x80\x94 Match 50\xc2\xb0 slot"));
            // Steering arc
            p.setPen(QPen(QColor(253,203,110),2.5)); p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(st[1].x-28,st[1].y-28,56,56), (int)(-st[1].a)*16, 35*16);
        } else if(m_step == 2) {
            // Instruction text
            p.setPen(annOrange);
            p.drawText(QPointF(W*0.02, curbY*0.18), QString::fromUtf8("\xe2\x9a\xa0 Slow entry \xe2\x80\x94 Watch both sides"));
            // Side gap lines
            p.setPen(QPen(annGreen,1.2,Qt::DashLine));
            p.drawLine(QPointF(st[2].x-cW/2-2, st[2].y), QPointF(st[2].x-cW/2-16, st[2].y));
            p.drawLine(QPointF(st[2].x+cW/2+2, st[2].y), QPointF(st[2].x+cW/2+16, st[2].y));
        } else if(m_step == 3) {
            // Instruction text
            p.setPen(annGreen);
            p.drawText(QPointF(W*0.02, curbY*0.18), QString::fromUtf8("\xe2\x9c\x85 Parked \xe2\x80\x94 Handbrake + Neutral"));
            p.setFont(QFont("Segoe UI",14)); p.setPen(annGreen);
            p.drawText(QPointF(st[3].x-cW/2-20, st[3].y+6), QString::fromUtf8("\xe2\x9c\x85"));
            // Checklist
            double hbX = W*0.70, hbY = curbY*0.25;
            p.setFont(QFont("Segoe UI",7,QFont::Bold)); p.setPen(annGreen);
            p.drawText(QPointF(hbX, hbY),    QString::fromUtf8("\xe2\x9c\x93 Handbrake"));
            p.drawText(QPointF(hbX, hbY+12), QString::fromUtf8("\xe2\x9c\x93 Neutral"));
        }

    // ════════════════════════════════════════════════════════════
    //  REVERSE (Marche arrière) — Annotated 4-step diagram
    // ════════════════════════════════════════════════════════════
    } else if (m_maneuver == 3) {
        double lL=W*0.36, lW=cW+24;
        // Lane
        p.setPen(Qt::NoPen); p.setBrush(QColor(210,215,225)); p.drawRect(QRectF(lL-14,0,lW+28,H));
        // Lane borders
        p.setPen(QPen(QColor(185,192,200),1.5,Qt::DashLine)); p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(lL,H*0.03),QPointF(lL,H*0.97));
        p.drawLine(QPointF(lL+lW,H*0.03),QPointF(lL+lW,H*0.97));
        // Lane labels
        p.setFont(QFont("Segoe UI",6,QFont::Bold)); p.setPen(QColor(160,170,180));
        p.drawText(QPointF(lL-10, H*0.5), "L"); p.drawText(QPointF(lL+lW+4, H*0.5), "R");

        // Center line of lane
        double cx=lL+lW/2;
        // Center dashes
        QPen cdash(QColor(255,255,255,120),1.5,Qt::CustomDashLine); cdash.setDashPattern({6,8});
        p.setPen(cdash); p.drawLine(QPointF(cx,H*0.03),QPointF(cx,H*0.97));

        double yy[]={H*0.10,H*0.33,H*0.56,H*0.80};
        for(int i=0;i<m_step&&i<4;i++) drawCar(p,cx,yy[i],180,cW,cH,ghostCol,false);
        int s=qMin(m_step,3); drawCar(p,cx,yy[s],180,cW,cH,myCol,true);
        if(m_step<3) drawArrow(p,cx,yy[s]+cH/2+5,cx,yy[s+1]-cH/2-5,QColor(0,184,148,130));

        // Direction of travel indicator
        p.setFont(QFont("Segoe UI",9,QFont::Bold)); p.setPen(QColor(225,112,85));
        p.drawText(QPointF(cx+cW/2+12, H*0.15), QString::fromUtf8("\xe2\xac\x87 Reverse"));

        // ── Step annotations ──
        QColor annBlue(9,132,227), annRed(225,112,85), annGreen(0,184,148), annOrange(230,126,34), annPurple(108,92,231);
        p.setFont(QFont("Segoe UI",8,QFont::Bold));
        if(m_step == 0) {
            // Instruction text
            p.setPen(annBlue);
            p.drawText(QPointF(lL+lW+18, H*0.08), QString::fromUtf8("\xf0\x9f\x91\x81 Check ALL mirrors"));
            p.drawText(QPointF(lL+lW+18, H*0.08+14), QString::fromUtf8("+ blind spots"));
            // Mirror check icons
            p.setFont(QFont("Segoe UI",10)); p.setPen(annBlue);
            p.drawText(QPointF(cx-6, yy[0]-cH/2-6), QString::fromUtf8("\xf0\x9f\x91\x81"));
            // Blind spot arcs
            p.setPen(QPen(annOrange,1.5,Qt::DashLine)); p.setBrush(QColor(230,126,34,15));
            p.drawPie(QRectF(cx-25,yy[0]-25,50,50), -160*16, -40*16);
            p.drawPie(QRectF(cx-25,yy[0]-25,50,50), -340*16, -40*16);
        } else if(m_step == 1) {
            // Instruction text
            p.setPen(annRed);
            p.drawText(QPointF(lL+lW+18, H*0.30), QString::fromUtf8("\xe2\xac\x87 Reverse slowly"));
            p.drawText(QPointF(lL+lW+18, H*0.30+14), QString::fromUtf8("Stay centered"));
            // Straight trajectory line
            p.setPen(QPen(annGreen,2,Qt::DashLine));
            p.drawLine(QPointF(cx, yy[1]-cH/2), QPointF(cx, yy[1]+cH/2+30));
            // Speed indicator
            p.setFont(QFont("Segoe UI",7,QFont::Bold)); p.setPen(annOrange);
            p.drawText(QPointF(lL-12, yy[1]), QString::fromUtf8("\xe2\x8f\xb1"));
        } else if(m_step == 2) {
            // Instruction text
            p.setPen(annOrange);
            p.drawText(QPointF(lL+lW+18, H*0.53), QString::fromUtf8("\xe2\x86\x94 Correct trajectory"));
            p.drawText(QPointF(lL+lW+18, H*0.53+14), QString::fromUtf8("Stay in lane"));
            // Correction arrows (small side movements)
            p.setPen(QPen(annOrange,2));
            p.drawLine(QPointF(cx-cW/2-2, yy[2]), QPointF(cx-cW/2-10, yy[2]));
            p.drawLine(QPointF(cx+cW/2+2, yy[2]), QPointF(cx+cW/2+10, yy[2]));
            // Lane boundary warnings
            p.setFont(QFont("Segoe UI",6,QFont::Bold)); p.setPen(annRed);
            p.drawText(QPointF(lL-10, yy[2]-10), QString::fromUtf8("\xe2\x9a\xa0"));
            p.drawText(QPointF(lL+lW+4, yy[2]-10), QString::fromUtf8("\xe2\x9a\xa0"));
        } else if(m_step == 3) {
            // Instruction text
            p.setPen(annGreen);
            p.drawText(QPointF(lL+lW+18, H*0.77), QString::fromUtf8("\xe2\x9c\x85 STOP \xe2\x80\x94 Secure"));
            p.setFont(QFont("Segoe UI",14)); p.setPen(annGreen);
            p.drawText(QPointF(cx-cW/2-20, yy[3]+6), QString::fromUtf8("\xe2\x9c\x85"));
            // Checklist
            p.setFont(QFont("Segoe UI",7,QFont::Bold)); p.setPen(annGreen);
            p.drawText(QPointF(lL+lW+18, H*0.77+18), QString::fromUtf8("\xe2\x9c\x93 Handbrake"));
            p.drawText(QPointF(lL+lW+18, H*0.77+30), QString::fromUtf8("\xe2\x9c\x93 Neutral"));
        }

    // ════════════════════════════════════════════════════════════
    //  U-TURN (Demi-tour) — Annotated 4-step diagram
    // ════════════════════════════════════════════════════════════
    } else if (m_maneuver == 4) {
        // Road with two curbs
        double topCurb = H*0.20, botCurb = H*0.80;
        p.setPen(Qt::NoPen); p.setBrush(QColor(210,215,225)); p.drawRect(QRectF(0,topCurb,W,botCurb-topCurb));
        // Top curb
        QLinearGradient cgt(0,topCurb-6,0,topCurb);
        cgt.setColorAt(0,QColor(145,150,158)); cgt.setColorAt(1,QColor(165,170,178));
        p.setBrush(cgt); p.drawRect(QRectF(0,topCurb-6,W,6));
        p.setBrush(QColor(232,236,240)); p.drawRect(QRectF(0,0,W,topCurb-6));
        // Bottom curb
        QLinearGradient cgb(0,botCurb,0,botCurb+6);
        cgb.setColorAt(0,QColor(165,170,178)); cgb.setColorAt(1,QColor(145,150,158));
        p.setBrush(cgb); p.drawRect(QRectF(0,botCurb,W,6));
        p.setBrush(QColor(232,236,240)); p.drawRect(QRectF(0,botCurb+6,W,H-botCurb-6));
        // Curb labels
        p.setFont(QFont("Segoe UI",6,QFont::Bold)); p.setPen(QColor(155,165,175));
        p.drawText(QPointF(W*0.02, topCurb-10), "Opposite curb");
        p.drawText(QPointF(W*0.02, botCurb+16), "Starting curb");

        // Center line
        QPen cl(QColor(255,255,255,150),2,Qt::CustomDashLine); cl.setDashPattern({6,8});
        p.setPen(cl); double midY = (topCurb+botCurb)/2;
        p.drawLine(QPointF(0,midY),QPointF(W,midY));

        // U-turn trajectory arc
        p.setPen(QPen(QColor(0,184,148,45),2.5,Qt::DashLine)); p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(W*0.15,topCurb-5,W*0.58,botCurb-topCurb+10),0,180*16);

        // Car positions
        struct Pos { double x,y,a; };
        Pos st[] = {
            {W*0.82, botCurb-20, 90},         // 0: start on right side
            {W*0.52, topCurb+20, 160},         // 1: forward + full left, near opposite curb
            {W*0.32, botCurb-20, 220},         // 2: reverse + full right, near starting curb
            {W*0.18, topCurb+30, 270}          // 3: forward in new direction
        };
        for(int i=0;i<m_step&&i<4;i++) drawCar(p,st[i].x,st[i].y,st[i].a,cW,cH,ghostCol,false);
        int s=qMin(m_step,3); drawCar(p,st[s].x,st[s].y,st[s].a,cW,cH,myCol,true);
        if(m_step<3) drawArrow(p,st[s].x-8,st[s].y,st[s+1].x+8,st[s+1].y,QColor(0,184,148,130));

        // ── Step annotations ──
        QColor annBlue(9,132,227), annRed(225,112,85), annGreen(0,184,148), annOrange(230,126,34), annPurple(108,92,231);
        p.setFont(QFont("Segoe UI",8,QFont::Bold));
        if(m_step == 0) {
            // Instruction text
            p.setPen(annBlue);
            p.drawText(QPointF(W*0.02, topCurb-18), QString::fromUtf8("\xf0\x9f\x91\x81 Check traffic \xe2\x80\x94 Signal LEFT"));
            // Position on right side
            p.setPen(QPen(annBlue,1.5,Qt::DashLine));
            p.drawLine(QPointF(st[0].x+cW/2+2, st[0].y), QPointF(st[0].x+cW/2+2, botCurb-2));
            p.setPen(annBlue); p.setFont(QFont("Segoe UI",7,QFont::Bold));
            p.drawText(QPointF(st[0].x+cW/2+6, (st[0].y+botCurb)/2), "~30cm");
            // Steering wheel
            double swX = W*0.12, swY = (topCurb+botCurb)/2;
            p.setPen(QPen(annPurple,3)); p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(swX, swY), 14, 14);
            p.setPen(QPen(annPurple,2.5));
            p.drawLine(QPointF(swX-11, swY+4), QPointF(swX+11, swY-4));
            p.setFont(QFont("Segoe UI",6,QFont::Bold)); p.setPen(annPurple);
            p.drawText(QRectF(swX-16, swY+16, 32, 12), Qt::AlignCenter, "FULL L");
        } else if(m_step == 1) {
            // Instruction text
            p.setPen(annRed);
            p.drawText(QPointF(W*0.02, topCurb-18), QString::fromUtf8("\xf0\x9f\x94\x84 FULL LEFT forward \xe2\x80\x94 Near opposite curb"));
            // Distance to opposite curb warning
            p.setPen(QPen(annRed,2,Qt::DashLine));
            p.drawLine(QPointF(st[1].x, st[1].y-cH/2), QPointF(st[1].x, topCurb+2));
            p.setPen(Qt::NoPen); p.setBrush(QColor(214,48,49,180));
            p.drawRoundedRect(QRectF(st[1].x-22, topCurb+2, 44, 16), 3, 3);
            p.setPen(Qt::white); p.setFont(QFont("Segoe UI",7,QFont::Bold));
            p.drawText(QRectF(st[1].x-22, topCurb+2, 44, 16), Qt::AlignCenter, "~1m STOP");
        } else if(m_step == 2) {
            // Instruction text
            p.setPen(annOrange);
            p.drawText(QPointF(W*0.02, topCurb-18), QString::fromUtf8("\xf0\x9f\x94\x84 FULL RIGHT reverse \xe2\x80\x94 Near starting curb"));
            // Distance to starting curb warning
            p.setPen(QPen(annRed,2,Qt::DashLine));
            p.drawLine(QPointF(st[2].x, st[2].y+cH/2), QPointF(st[2].x, botCurb-2));
            p.setPen(Qt::NoPen); p.setBrush(QColor(214,48,49,180));
            p.drawRoundedRect(QRectF(st[2].x-22, botCurb-18, 44, 16), 3, 3);
            p.setPen(Qt::white); p.setFont(QFont("Segoe UI",7,QFont::Bold));
            p.drawText(QRectF(st[2].x-22, botCurb-18, 44, 16), Qt::AlignCenter, "~1m STOP");
            // Steering wheel
            double swX = W*0.12, swY = (topCurb+botCurb)/2;
            p.setPen(QPen(annPurple,3)); p.setBrush(Qt::NoBrush);
            p.drawEllipse(QPointF(swX, swY), 14, 14);
            p.setPen(QPen(annPurple,2.5));
            p.drawLine(QPointF(swX+11, swY-4), QPointF(swX-11, swY+4));
            p.setFont(QFont("Segoe UI",6,QFont::Bold)); p.setPen(annPurple);
            p.drawText(QRectF(swX-16, swY+16, 32, 12), Qt::AlignCenter, "FULL R");
        } else if(m_step == 3) {
            // Instruction text
            p.setPen(annGreen);
            p.drawText(QPointF(W*0.02, topCurb-18), QString::fromUtf8("\xe2\x9c\x85 Complete \xe2\x80\x94 Now facing opposite direction"));
            p.setFont(QFont("Segoe UI",14)); p.setPen(annGreen);
            p.drawText(QPointF(st[3].x-cW/2-20, st[3].y+6), QString::fromUtf8("\xe2\x9c\x85"));
            // New direction arrow
            p.setPen(QPen(annGreen,2));
            p.drawLine(QPointF(st[3].x, st[3].y-cH/2-2), QPointF(st[3].x, st[3].y-cH/2-20));
            QPolygonF da; da << QPointF(st[3].x, st[3].y-cH/2-22) << QPointF(st[3].x-5, st[3].y-cH/2-14) << QPointF(st[3].x+5, st[3].y-cH/2-14);
            p.setBrush(annGreen); p.setPen(Qt::NoPen); p.drawPolygon(da);
        }

    // ════════════════════════════════════════════════════════════
    //  FORWARD CORRIDOR (Marche avant) — Annotated 5-step diagram
    //  Bird's-eye view: car drives left through a narrow corridor
    // ════════════════════════════════════════════════════════════
    } else if (m_maneuver == 5) {
        // ════════════════════════════════════════════════════════════
        //  FORWARD CORRIDOR (Marche avant) — 3D PERSPECTIVE VIEW
        // ════════════════════════════════════════════════════════════
        double cx = W / 2.0;   // Vanishing point X
        double cy = H * 0.40;  // Vanishing point Y (horizon)

        // Draw sky / distant background
        QLinearGradient sky(0, 0, 0, cy);
        sky.setColorAt(0, QColor(200, 220, 240));
        sky.setColorAt(1, QColor(240, 245, 250));
        p.setBrush(sky);
        p.setPen(Qt::NoPen);
        p.drawRect(QRectF(0, 0, W, cy));

        // Draw ground (floor)
        QLinearGradient ground(0, cy, 0, H);
        ground.setColorAt(0, QColor(160, 165, 175));
        ground.setColorAt(1, QColor(130, 135, 145));
        p.setBrush(ground);
        p.drawRect(QRectF(0, cy, W, H - cy));

        // Left wall polygon
        QPolygonF leftWall;
        double wl_top = cx - W * 0.15;
        double wr_top = cx + W * 0.15;
        leftWall << QPointF(0, H) << QPointF(wl_top, cy) << QPointF(wl_top, cy-30) << QPointF(0, cy-120);
        p.setBrush(QColor(180, 185, 195));
        p.setPen(QPen(QColor(150, 155, 165), 1));
        p.drawPolygon(leftWall);

        // Right wall polygon
        QPolygonF rightWall;
        rightWall << QPointF(W, H) << QPointF(wr_top, cy) << QPointF(wr_top, cy-30) << QPointF(W, cy-120);
        p.setBrush(QColor(170, 175, 185)); // slightly varied for lighting
        p.drawPolygon(rightWall);

        // Grid lines on the ground to emphasize 3D perspective
        p.setPen(QPen(QColor(255, 255, 255, 40), 1.5, Qt::DashLine));
        for (int i = 1; i <= 5; i++) {
            double ly = cy + (H - cy) * (i / 5.0) * (i / 5.0);
            p.drawLine(QPointF(0, ly), QPointF(W, ly));
        }

        // Perspective side lines (corridor edges)
        p.setPen(QPen(QColor(0, 184, 148, 150), 3, Qt::SolidLine));
        p.drawLine(QPointF(wl_top, cy), QPointF(0, H));
        p.drawLine(QPointF(wr_top, cy), QPointF(W, H));

        // Warning traffic cones along the walls
        p.setPen(Qt::NoPen);
        for(int i = 0; i <= 6; i++) {
            double depth = qMax(0.05, i / 6.0); // 1 = closest, 0 = vanishing point
            double xL = cx - (cx * depth);
            double xR = cx + ((W - cx) * depth);
            double yPos = cy + (H - cy) * depth;
            
            double heightCone = 30 * depth;
            double widthCone = 12 * depth;
            
            // Left Cone
            p.setBrush(QColor(230, 80, 30));
            p.drawEllipse(QPointF(xL, yPos), widthCone, heightCone/3);
            QPolygonF coneL;
            coneL << QPointF(xL - widthCone, yPos) << QPointF(xL + widthCone, yPos) << QPointF(xL, yPos - heightCone);
            p.drawPolygon(coneL);
            p.setBrush(QColor(255, 255, 255));
            p.drawRect(QRectF(xL - widthCone*0.6, yPos - heightCone*0.6, widthCone*1.2, heightCone*0.25));

            // Right Cone
            p.setBrush(QColor(230, 80, 30));
            p.drawEllipse(QPointF(xR, yPos), widthCone, heightCone/3);
            QPolygonF coneR;
            coneR << QPointF(xR - widthCone, yPos) << QPointF(xR + widthCone, yPos) << QPointF(xR, yPos - heightCone);
            p.drawPolygon(coneR);
            p.setBrush(QColor(255, 255, 255));
            p.drawRect(QRectF(xR - widthCone*0.6, yPos - heightCone*0.6, widthCone*1.2, heightCone*0.25));
        }

        // ── 3D Car Placeholder (Back view) ──
        double progress = m_totalSteps > 1 ? (double)qMin(m_step, m_totalSteps-1) / (double)(m_totalSteps - 1) : 0;
        
        // Depth mapping: 1.0 = near camera, 0.2 = far away
        double cDepth = 1.0 - (progress * 0.75);
        if (m_step == 4) cDepth = 0.15; // Drive off into the distance

        double carW = 140 * cDepth;
        double carH = 90 * cDepth;
        double carY = cy + (H - cy) * cDepth - carH/2;
        
        // Car shadow
        p.setBrush(QColor(0, 0, 0, 70));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(cx, carY + carH/2 + carH*0.1), carW*0.6, carH*0.15);

        // Car Body (back)
        p.setBrush(QColor(0, 184, 148));
        p.setPen(QPen(QColor(0, 140, 110), 1.5));
        p.drawRoundedRect(QRectF(cx - carW/2, carY - carH/2, carW, carH), 8*cDepth, 8*cDepth);
        
        // Rear window
        p.setBrush(QColor(30, 40, 50));
        p.drawRoundedRect(QRectF(cx - carW*0.4, carY - carH*0.4, carW*0.8, carH*0.35), 4*cDepth, 4*cDepth);
        
        // Taillights
        p.setBrush(QColor(240, 50, 30));
        p.drawRoundedRect(QRectF(cx - carW*0.45, carY + carH*0.1, carW*0.2, carH*0.15), 3*cDepth, 3*cDepth);
        p.drawRoundedRect(QRectF(cx + carW*0.25, carY + carH*0.1, carW*0.2, carH*0.15), 3*cDepth, 3*cDepth);
        // Brake light glow if stopped
        if (m_step == 0 || m_step == 4) {
            p.setBrush(QColor(255, 50, 20, 100));
            p.drawEllipse(QPointF(cx - carW*0.35, carY + carH*0.17), carW*0.3, carH*0.3);
            p.drawEllipse(QPointF(cx + carW*0.35, carY + carH*0.17), carW*0.3, carH*0.3);
        }

        // License plate
        p.setBrush(QColor(255, 255, 255));
        p.setPen(QPen(QColor(0,0,0), 0.5));
        p.drawRect(QRectF(cx - carW*0.15, carY + carH*0.15, carW*0.3, carH*0.1));

        // Sensors radar waves
        if (m_step > 0 && m_step < 4) {
            p.setPen(QPen(QColor(241, 196, 15, 200), 2*cDepth, Qt::SolidLine));
            p.setBrush(Qt::NoBrush);
            p.drawArc(QRectF(cx - carW/2 - 20*cDepth, carY - 10*cDepth, 20*cDepth, 20*cDepth), 90*16, 180*16);
            p.drawArc(QRectF(cx + carW/2, carY - 10*cDepth, 20*cDepth, 20*cDepth), -90*16, 180*16);
        }

        // Overlay text or annotations for 3D view
        p.setFont(QFont("Segoe UI", 12, QFont::Bold));
        p.setPen(QColor(0, 184, 148));
        p.drawText(QRectF(0, 10, W, 30), Qt::AlignCenter, "3D CORRIDOR VIEW");
        
        p.setFont(QFont("Segoe UI", 9, QFont::Bold));
        if (m_step == 0) {
            p.setPen(QColor(225, 112, 85));
            p.drawText(QRectF(0, H - 25, W, 20), Qt::AlignCenter, "Waiting at Entry...");
        } else if (m_step == 4) {
            p.setPen(QColor(0, 200, 100));
            p.drawText(QRectF(0, H - 25, W, 20), Qt::AlignCenter, "\xe2\x9c\x85 SUCCESS - Corridor Cleared!");
        } else {
            p.setPen(QColor(9, 132, 227));
            p.drawText(QRectF(0, H - 25, W, 20), Qt::AlignCenter, "Driving through (Keep Centered)");
        }
    }

    // Labels
    p.setFont(QFont("Segoe UI",8,QFont::Bold));
    p.setPen(QColor(100,110,120));
    p.drawText(QRectF(W-85,H-20,75,16), Qt::AlignRight, QString("Step %1/%2").arg(m_step+1).arg(m_totalSteps));
    QStringList names={"Parallel","Perpendicular","Diagonal","Reverse","U-turn","Fwd Corridor"};
    if(m_maneuver>=0&&m_maneuver<names.size()){
        QFont nf("Segoe UI",8,QFont::Bold); p.setFont(nf);
        QFontMetrics fm(nf); int tw=fm.horizontalAdvance(names[m_maneuver])+16;
        p.setPen(Qt::NoPen); p.setBrush(QColor(0,184,148,20));
        p.drawRoundedRect(QRectF(W-88-tw,H-22,tw,18),5,5);
        p.setPen(QColor(0,184,148));
        p.drawText(QRectF(W-88-tw,H-22,tw,18),Qt::AlignCenter,names[m_maneuver]);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  CONSTRUCTOR
// ═══════════════════════════════════════════════════════════════════

ParkingWidget::ParkingWidget(const QString &userName, const QString &userRole,
                             int userId, QWidget *parent)
    : QWidget(parent), m_userName(userName), m_userRole(userRole),
      m_isMoniteur(userRole == "Moniteur"), m_sessionSeconds(0),
      m_sessionActive(false), m_examMode(false), m_freeTrainingMode(false),
      m_fullExamMode(false), m_fullExamPhase(0),
      m_currentManeuver(0), m_currentStep(0), m_totalSteps(0),
      m_maquetteConnected(false), m_simulationMode(false),
      m_currentSessionId(-1), m_currentEleveId(1), m_currentVehiculeId(1),
      m_nbErrors(0), m_nbCalages(0), m_contactObstacle(false),
      m_pendingStep(-1), m_simEventIndex(0),
      m_totalXP(0), m_currentLevel(1), m_practiceStreak(0),
      m_totalSessionsCount(0), m_totalSuccessCount(0), m_bestTimeSeconds(999),
      m_selectedPlate("215 TU 8842"), m_selectedModel("Renault Clio V"),
      m_selectedCarLabel(nullptr), m_floatingMsg(nullptr), m_stepDiagram(nullptr),
      m_statsFAB(nullptr), m_vehStatsFAB(nullptr), m_vehPage(nullptr),
      m_aiScrollArea(nullptr), m_aiChatContent(nullptr), m_aiChatLayout(nullptr),
      m_aiInput(nullptr), m_aiTypingLabel(nullptr), m_aiApiKeyBtn(nullptr),
      m_aiProc(nullptr), m_aiTmpPayload(nullptr), m_aiPrevPage(1),
      m_notesFAB(nullptr), m_notesBubble(nullptr), m_notesTextEdit(nullptr),
      m_notesBubbleTitle(nullptr), m_notesSaveStatus(nullptr), m_notesAutoSave(nullptr),
      m_notesBubbleParent(nullptr), m_notesBubbleOpen(false),
      m_realExamPhase(EXAM_IDLE), m_examDelayTimer(nullptr),
      m_examBigIcon(nullptr), m_examBigTitle(nullptr), m_examBigInstruction(nullptr),
      m_examInstructionCard(nullptr), m_realExamErrors(0),
      m_selectedTarif(15.0), m_totalDepense(0.0), m_examReady(false),
      m_examReadyLabel(nullptr), m_reservationBtn(nullptr),
      m_depenseLabel(nullptr), m_reservationsLayout(nullptr),
      m_lastDistFront(-1), m_lastDistLeft(-1), m_lastDistRight(-1),
      m_lastDistRearL(-1), m_lastDistRearR(-1),
      m_voiceEnabled(true), m_voiceToggle(nullptr),
      m_sensorSpeakCooldown(nullptr), m_tts(nullptr),
      m_mistakeTitle(nullptr), m_mistakeDesc(nullptr), m_mistakeTip(nullptr),
      m_weatherWidget(nullptr), m_trendWidget(nullptr),
      m_recommendLabel(nullptr), m_recommendBtn(nullptr), m_recommendedManeuver(0),
      m_simButtonPanel(nullptr), m_dbMsgLabel(nullptr)
{
    m_maneuverSuccessCounts = {0,0,0,0,0,0}; // index 5 = marche_avant (Full Exam)
    m_maneuverAttemptCounts = {0,0,0,0,0,0};
    m_sessionTimer = new QTimer(this);
    m_examDelayTimer = new QTimer(this);
    m_examDelayTimer->setSingleShot(true);
    connect(m_sessionTimer, &QTimer::timeout, this, &ParkingWidget::updateSessionTimer);
    m_delayTimer = new QTimer(this);
    m_delayTimer->setSingleShot(true);
    connect(m_delayTimer, &QTimer::timeout, this, &ParkingWidget::onDelayedAction);

    m_floatingTimer = new QTimer(this);
    m_floatingTimer->setSingleShot(true);
    connect(m_floatingTimer, &QTimer::timeout, this, [this](){
        if(m_floatingMsg) m_floatingMsg->setVisible(false);
    });

    m_sensorMgr = new SensorManager(this);
    connectSensorSignals();
    m_simEvents = {"RIGHT_OBSTACLE","LEFT_OBSTACLE","LEFT_SENSOR1","RIGHT_SENSOR2",
                   "REAR_RIGHT_SENSOR","BOTH_REAR_OBSTACLE","BOTH_REAR_SENSOR"};

    // ── Voice Assistant (QTextToSpeech) ──
    m_tts = new QTextToSpeech(this);
    m_voiceEnabled = true;
    // Set English locale for voice
    bool localeSet = false;
    for (const auto &locale : m_tts->availableLocales()) {
        if (locale.language() == QLocale::English) {
            m_tts->setLocale(locale);
            localeSet = true;
            break;
        }
    }
    if (!localeSet) {
        m_tts->setLocale(QLocale(QLocale::English, QLocale::UnitedStates));
    }
    m_tts->setRate(0.0);    // normal speed for English
    m_tts->setVolume(0.9);
    m_sensorSpeakCooldown = new QTimer(this);
    m_sensorSpeakCooldown->setSingleShot(true);

    // La connexion Oracle est deja initialisee dans main.cpp
    // ParkingDBManager::instance().initialize() n'est plus necessaire ici
    initManeuverData();

    // Use the ID resolved at login time — guaranteed to exist in Oracle
    if (!m_isMoniteur && userId > 0) {
        m_currentEleveId = userId;
        qDebug() << "[Parking] m_currentEleveId set from login:" << m_currentEleveId;
    }

    initCommonMistakes();
    initVideoTutorials();
    setupUI();
    loadStatsFromDB();
    loadHistoryFromDB();
    updateDashboard();
}

ParkingWidget::~ParkingWidget()
{
    if(m_sessionTimer && m_sessionTimer->isActive()) m_sessionTimer->stop();
    if(m_delayTimer && m_delayTimer->isActive()) m_delayTimer->stop();
    if(m_floatingTimer && m_floatingTimer->isActive()) m_floatingTimer->stop();
    if(m_sensorSpeakCooldown && m_sensorSpeakCooldown->isActive()) m_sensorSpeakCooldown->stop();
    if(m_tts) m_tts->stop();
}

void ParkingWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    repositionNotesBubble();
}

// ═══════════════════════════════════════════════════════════════════
//  LOAD STATS FROM DATABASE (Persistence)
// ═══════════════════════════════════════════════════════════════════

void ParkingWidget::loadStatsFromDB()
{
    auto &db = ParkingDBManager::instance();
    m_totalSessionsCount = db.getSessionCount(m_currentEleveId);
    m_totalSuccessCount = db.getSuccessCount(m_currentEleveId);

    // Load best time across all maneuvers
    m_bestTimeSeconds = 999;
    for(int i = 0; i < m_maneuverDbKeys.size(); i++){
        int bt = db.getMeilleurTemps(m_currentEleveId, m_maneuverDbKeys[i]);
        if(bt > 0 && bt < m_bestTimeSeconds) m_bestTimeSeconds = bt;
    }

    // Load per-maneuver stats — UNION both new PARKING_SESSIONS and legacy SESSIONS
    for(int i = 0; i < 5; i++){
        QSqlQuery q(QSqlDatabase::database());
        // Attempts: count from both tables
        q.prepare("SELECT COUNT(*) FROM ("
                  "SELECT session_id FROM PARKING_SESSIONS WHERE student_id=:id1 AND maneuver_type=:type1 "
                  "UNION ALL "
                  "SELECT ID FROM SESSIONS WHERE ELEVE_ID=:id2 AND MANOEUVRE_TYPE=:type2"
                  ")");
        q.bindValue(":id1", m_currentEleveId); q.bindValue(":type1", m_maneuverDbKeys[i]);
        q.bindValue(":id2", m_currentEleveId); q.bindValue(":type2", m_maneuverDbKeys[i]);
        if(q.exec() && q.next()) m_maneuverAttemptCounts[i] = q.value(0).toInt();

        // Successes: count from both tables
        q.prepare("SELECT COUNT(*) FROM ("
                  "SELECT session_id FROM PARKING_SESSIONS WHERE student_id=:id1 AND maneuver_type=:type1 AND is_successful=1 "
                  "UNION ALL "
                  "SELECT ID FROM SESSIONS WHERE ELEVE_ID=:id2 AND MANOEUVRE_TYPE=:type2 AND REUSSI=1"
                  ")");
        q.bindValue(":id1", m_currentEleveId); q.bindValue(":type1", m_maneuverDbKeys[i]);
        q.bindValue(":id2", m_currentEleveId); q.bindValue(":type2", m_maneuverDbKeys[i]);
        if(q.exec() && q.next()) m_maneuverSuccessCounts[i] = q.value(0).toInt();
    }

    // Estimate XP and level
    m_totalXP = m_totalSuccessCount * 25 + (m_totalSessionsCount - m_totalSuccessCount) * 10;
    m_currentLevel = 1 + m_totalXP / 100;

    // Practice streak (count consecutive days with sessions)
    m_practiceStreak = 0;
    QSqlQuery sq(QSqlDatabase::database());

    // Load total spent
    m_totalDepense = db.getTotalDepense(m_currentEleveId);

    // Check exam readiness
    checkExamReadiness();

    sq.prepare("SELECT * FROM (SELECT DISTINCT TRUNC(session_start) AS d FROM PARKING_SESSIONS WHERE student_id=? "
               "ORDER BY d DESC) WHERE ROWNUM <= 30");
    sq.addBindValue(m_currentEleveId);
    if(sq.exec()){
        QDate prev = QDate::currentDate().addDays(1);
        while(sq.next()){
            QDate d = QDate::fromString(sq.value(0).toString(), "yyyy-MM-dd");
            if(!d.isValid()) continue;
            if(prev.addDays(-1) == d || prev == d || prev.addDays(1) == d){
                // Allow today or yesterday gap
                if(d == QDate::currentDate() || d == QDate::currentDate().addDays(-1) || m_practiceStreak > 0){
                    m_practiceStreak++;
                    prev = d;
                } else break;
            } else break;
        }
    }
}

void ParkingWidget::loadHistoryFromDB()
{
    refreshHistoryUI();
}

void ParkingWidget::refreshHistoryUI()
{
    if(!m_historyLayout) return;
    // Clear existing items
    QLayoutItem *item;
    while((item = m_historyLayout->takeAt(0)) != nullptr){
        if(item->widget()) delete item->widget();
        delete item;
    }

    auto sessions = ParkingDBManager::instance().getSessionsEleve(m_currentEleveId, 8);
    if(sessions.isEmpty()){
        QLabel *empty = new QLabel(QString::fromUtf8(
            "\xf0\x9f\x93\xad  No sessions yet\nStart your first practice!"), this);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet("QLabel{font-size:11px;color:#b2bec3;background:#f8f8f8;border-radius:10px;padding:16px;border:none;}");
        m_historyLayout->addWidget(empty);
        return;
    }

    for(const auto &s : sessions){
        QWidget *row = new QWidget(this);
        row->setStyleSheet("background:transparent;");
        QHBoxLayout *rl = new QHBoxLayout(row);
        rl->setContentsMargins(8,4,8,4);
        rl->setSpacing(8);

        bool reussi = s["reussi"].toBool();
        int duree = s["duree_secondes"].toInt();
        QString manType = s["manoeuvre_type"].toString();
        QString date = s["date_debut"].toString().left(16);
        bool examMode = s.contains("mode_examen") && s["mode_examen"].toBool();

        // Find icon for maneuver (manType comes from DB — uses db keys like "creneau")
        QString icon = QString::fromUtf8("\xf0\x9f\x94\xb2");
        for(int i = 0; i < m_maneuverDbKeys.size(); i++){
            if(m_maneuverDbKeys[i] == manType){ icon = m_maneuverIcons[i]; break; }
        }

        // Status dot
        QLabel *dot = new QLabel(reussi ? QString::fromUtf8("\xe2\x9c\x85") :
                                          QString::fromUtf8("\xe2\x9d\x8c"), row);
        dot->setFixedSize(20,20);
        dot->setAlignment(Qt::AlignCenter);
        dot->setStyleSheet("QLabel{font-size:12px;background:transparent;border:none;}");
        rl->addWidget(dot);

        // Maneuver name
        QLabel *nm = new QLabel(icon + " " + manType, row);
        nm->setStyleSheet("QLabel{font-size:10px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
        rl->addWidget(nm, 1);

        // Exam badge
        if(examMode){
            QLabel *exBadge = new QLabel(QString::fromUtf8("\xf0\x9f\x8e\x93"), row);
            exBadge->setFixedSize(16,16);
            exBadge->setStyleSheet("QLabel{font-size:10px;background:transparent;border:none;}");
            rl->addWidget(exBadge);
        }

        // Duration
        int mn = duree/60, sc = duree%60;
        QLabel *dur = new QLabel(QString("%1:%2")
            .arg(mn,2,10,QChar('0')).arg(sc,2,10,QChar('0')), row);
        dur->setStyleSheet("QLabel{font-size:9px;color:#636e72;font-family:'Courier New',monospace;"
            "background:transparent;border:none;}");
        rl->addWidget(dur);

        // Date
        QLabel *dt = new QLabel(date, row);
        dt->setStyleSheet("QLabel{font-size:8px;color:#b2bec3;background:transparent;border:none;}");
        rl->addWidget(dt);

        m_historyLayout->addWidget(row);

        // Separator
        if(m_historyLayout->count() < 16){
            QFrame *sep = new QFrame(this);
            sep->setFrameShape(QFrame::HLine);
            sep->setStyleSheet("QFrame{background:rgba(0,0,0,0.04);max-height:1px;border:none;}");
            m_historyLayout->addWidget(sep);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
//  SETUP UI  —  2 pages only: Home → Session
// ═══════════════════════════════════════════════════════════════════

void ParkingWidget::setupUI()
{
    QVBoxLayout *mainL = new QVBoxLayout(this);
    mainL->setContentsMargins(0,0,0,0);
    mainL->setSpacing(0);
    mainL->addWidget(createBanner());

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(createVehicleSelectionPage()); // 0 — Vehicle selection
    m_stack->addWidget(createHomePage());              // 1 — Maneuver picker
    m_stack->addWidget(createSessionPage());           // 2 — Active session
    m_stack->addWidget(createReservationPage());       // 3 — Booking examen
    m_stack->addWidget(createAnalyticsPage());         // 4 — Analytics & Smart Coach
    m_stack->addWidget(createAssistantPage());         // 5 — AI Assistant
    m_stack->setCurrentIndex(0);
    mainL->addWidget(m_stack, 1);

    // Initial positioning of vehicle-page stats FAB (before first resizeEvent)
    QTimer::singleShot(0, this, [this](){
        repositionNotesBubble();
    });
}

QWidget* ParkingWidget::createBanner()
{
    QWidget *b = new QWidget(this);
    b->setFixedHeight(52);
    b->setObjectName("pkBanner");
    b->setStyleSheet("QWidget#pkBanner{background:white;border-bottom:2px solid #00b894;}");
    QHBoxLayout *l = new QHBoxLayout(b);
    l->setContentsMargins(24,0,24,0);
    l->setSpacing(10);

    QLabel *icon = new QLabel(QString::fromUtf8("\xf0\x9f\x85\xbf\xef\xb8\x8f"), this);
    icon->setFixedSize(32,32);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet("QLabel{font-size:16px;background:#00b894;border-radius:16px;border:none;}");
    l->addWidget(icon);

    QLabel *t = new QLabel("PARKING", this);
    t->setStyleSheet("QLabel{color:#2d3436;font-size:14px;font-weight:bold;"
        "letter-spacing:3px;background:transparent;border:none;}");
    l->addWidget(t);

    QLabel *sub = new QLabel("Autonomous Training", this);
    sub->setStyleSheet("QLabel{color:#b2bec3;font-size:10px;background:transparent;border:none;}");
    l->addWidget(sub);
    l->addStretch();

    // ── AI Assistant button (banner, top-right) ──
    // 🤖  U+1F916  →  UTF-8 F0 9F A4 96
    m_aiApiKeyBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\xa4\x96  AI"), this);
    m_aiApiKeyBtn->setFixedHeight(30);
    m_aiApiKeyBtn->setCursor(Qt::PointingHandCursor);
    m_aiApiKeyBtn->setToolTip("AI Assistant");
    m_aiApiKeyBtn->setStyleSheet(
        "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "  stop:0 #6c5ce7, stop:1 #a29bfe);"
        "  color:white;font-size:11px;font-weight:bold;border:none;"
        "  border-radius:15px;padding:0 14px;}"
        "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "  stop:0 #7d6ff0, stop:1 #b2a8ff);}"
        "QPushButton:pressed{background:#5a4bd1;}"
    );
    connect(m_aiApiKeyBtn, &QPushButton::clicked, this, [this](){
        m_aiPrevPage = m_stack->currentIndex();
        m_stack->setCurrentIndex(5);
    });
    l->addWidget(m_aiApiKeyBtn);
    l->addSpacing(8);

    // Car label exists but hidden from banner (shown in home page hero)
    m_selectedCarLabel = new QLabel("", this);
    m_selectedCarLabel->setVisible(false);

    m_dashLevel = new QLabel("Lv. 1", this);
    m_dashLevel->setStyleSheet("QLabel{color:white;font-size:10px;font-weight:bold;"
        "background:#00b894;border-radius:10px;padding:4px 12px;border:none;}");
    l->addWidget(m_dashLevel);

    m_dashXPBar = new QProgressBar(this);
    m_dashXPBar->setRange(0,100); m_dashXPBar->setValue(0);
    m_dashXPBar->setFixedSize(70,4); m_dashXPBar->setTextVisible(false);
    m_dashXPBar->setStyleSheet("QProgressBar{background:#e2e8f0;border:none;border-radius:2px;}"
        "QProgressBar::chunk{background:#00b894;border-radius:2px;}");
    l->addWidget(m_dashXPBar);

    m_dashXPLabel = new QLabel("0 XP", this);
    m_dashXPLabel->setStyleSheet("QLabel{font-size:8px;color:#b2bec3;background:transparent;border:none;}");
    l->addWidget(m_dashXPLabel);

    return b;
}

// ═══════════════════════════════════════════════════════════════════
//  VEHICLE SELECTION PAGE — Choose an equipped car before training
// ═══════════════════════════════════════════════════════════════════

QWidget* ParkingWidget::createVehicleSelectionPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName("vehPage");
    page->setStyleSheet(QString("QWidget#vehPage{%1}").arg(BG));

    QVBoxLayout *pL = new QVBoxLayout(page);
    pL->setContentsMargins(0,0,0,0);
    pL->setSpacing(0);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(SCROLL_SS);

    QWidget *content = new QWidget(this);
    content->setObjectName("vehContent");
    content->setStyleSheet("QWidget#vehContent{background:#f0f2f5;}");
    QVBoxLayout *cL = new QVBoxLayout(content);
    cL->setContentsMargins(36,32,36,32);
    cL->setSpacing(24);

    // ── Hero Header ──
    QFrame *heroCard = new QFrame(this);
    heroCard->setObjectName("heroCard");
    heroCard->setStyleSheet(
        "QFrame#heroCard{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "stop:0 #e8f8f5,stop:1 #dfe6e9);border-radius:20px;border:1px solid #e2e8f0;}");
    heroCard->setGraphicsEffect(shadow(heroCard, 24, 12));

    QVBoxLayout *heroL = new QVBoxLayout(heroCard);
    heroL->setContentsMargins(36,28,36,28);
    heroL->setSpacing(12);

    m_vehiclePageTitle = new QLabel(QString::fromUtf8("Select your vehicle"), this);
    m_vehiclePageTitle->setAlignment(Qt::AlignCenter);
    m_vehiclePageTitle->setStyleSheet("QLabel{font-size:24px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;font-family:'Segoe UI',sans-serif;}");
    m_vehiclePageTitle->setWordWrap(true);
    heroL->addWidget(m_vehiclePageTitle);

    QLabel *subtitle = new QLabel(QString::fromUtf8(
        "Vehicles equipped with ultrasonic sensors and parking assistance.\n"
        "Practice on your own or take the ATTT exam."), this);
    subtitle->setAlignment(Qt::AlignCenter);
    subtitle->setStyleSheet("QLabel{font-size:12px;color:#636e72;background:transparent;"
        "border:none;line-height:1.6;}");
    subtitle->setWordWrap(true);
    heroL->addWidget(subtitle);

    // Feature pills
    QHBoxLayout *pillsRow = new QHBoxLayout();
    pillsRow->setSpacing(10);
    pillsRow->addStretch();

    auto makePill = [this](const QString &icon, const QString &text) -> QLabel* {
        QLabel *pill = new QLabel(icon + "  " + text, this);
        pill->setStyleSheet("QLabel{font-size:10px;color:#00b894;background:white;"
            "border-radius:14px;padding:7px 16px;border:1px solid #d0f0e8;font-weight:bold;}");
        return pill;
    };
    pillsRow->addWidget(makePill(QString::fromUtf8("📡"), QString::fromUtf8("Real-time sensors")));
    pillsRow->addWidget(makePill(QString::fromUtf8("🤖"), "Voice guidance"));
    pillsRow->addWidget(makePill(QString::fromUtf8("🎓"), "ATTT exam mode"));
    pillsRow->addStretch();
    heroL->addLayout(pillsRow);

    cL->addWidget(heroCard);

    // ── Section Title ──
    QLabel *secTitle = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x9a\x98  Available vehicles"), this);
    secTitle->setStyleSheet("QLabel{font-size:16px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
    cL->addWidget(secTitle);

    // ── Vehicle Grid (2 columns) ──
    QGridLayout *vehGrid = new QGridLayout();
    vehGrid->setSpacing(16);
    m_vehicleListLayout = new QVBoxLayout(); // keep for compat but unused

    // Load vehicles from database
    QList<QVariantMap> vehicles = ParkingDBManager::instance().getVehiculesDisponibles();

    if(vehicles.isEmpty()){
        QLabel *empty = new QLabel(QString::fromUtf8(
            "\xe2\x9a\xa0\xef\xb8\x8f No vehicles available at the moment.\nContact administration."), this);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet("QLabel{font-size:13px;color:#e17055;background:#fef3e2;border-radius:14px;padding:24px;border:none;}");
        empty->setWordWrap(true);
        vehGrid->addWidget(empty, 0, 0, 1, 2);
    } else {
        for(int idx = 0; idx < vehicles.size(); idx++){
            const auto &v = vehicles[idx];
            int vId = v["id"].toInt();
            QString immat = v["immatriculation"].toString();
            QString modele = v["modele"].toString();
            QString typeAssist = v["type_assistance"].toString();
            int nbSensors = v["nb_capteurs"].toInt();
            QString descSensors = v["description_capteurs"].toString();
            QString port = v["port_serie"].toString();

            QFrame *card = new QFrame(this);
            QString cid = QString("vehCard%1").arg(vId);
            card->setObjectName(cid);
            card->setStyleSheet(QString(
                "QFrame#%1{background:white;border-radius:16px;border:1px solid #e2e8f0;}"
                "QFrame#%1:hover{border-color:#00b894;}").arg(cid));
            card->setGraphicsEffect(shadow(card, 16, 10));
            card->setCursor(Qt::PointingHandCursor);

            QVBoxLayout *cardL = new QVBoxLayout(card);
            cardL->setContentsMargins(22,20,22,20);
            cardL->setSpacing(14);

            // Top Row: car icon + name + status
            QHBoxLayout *topRow = new QHBoxLayout();
            topRow->setSpacing(14);

            QLabel *carIcon = new QLabel(QString::fromUtf8("\xf0\x9f\x9a\x97"), card);
            carIcon->setFixedSize(48,48);
            carIcon->setAlignment(Qt::AlignCenter);
            carIcon->setStyleSheet("QLabel{background:#e8f8f5;border-radius:12px;font-size:24px;border:none;}");
            topRow->addWidget(carIcon);

            QVBoxLayout *infoCol = new QVBoxLayout();
            infoCol->setSpacing(3);

            QLabel *modelLabel = new QLabel(modele, card);
            modelLabel->setStyleSheet("QLabel{font-size:16px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
            infoCol->addWidget(modelLabel);

            QLabel *platLabel = new QLabel(QString::fromUtf8("\xf0\x9f\x93\x8b  ") + immat, card);
            platLabel->setStyleSheet("QLabel{font-size:12px;color:#636e72;background:transparent;border:none;}");
            infoCol->addWidget(platLabel);

            topRow->addLayout(infoCol, 1);

            // Ready badge
            QLabel *readyBadge = new QLabel(QString::fromUtf8("\xe2\x9c\x85 Ready"), card);
            readyBadge->setStyleSheet("QLabel{font-size:10px;font-weight:bold;color:#00b894;"
                "background:#e6fff9;border-radius:10px;padding:4px 12px;border:none;}");
            topRow->addWidget(readyBadge, 0, Qt::AlignTop);
            cardL->addLayout(topRow);

            // Separator
            QFrame *sep = new QFrame(card);
            sep->setFrameShape(QFrame::HLine);
            sep->setStyleSheet("QFrame{background:rgba(0,184,148,0.08);max-height:1px;border:none;}");
            cardL->addWidget(sep);

            // Equipment Row
            QHBoxLayout *equipRow = new QHBoxLayout();
            equipRow->setSpacing(8);

            QLabel *assistLabel = new QLabel(QString::fromUtf8("\xf0\x9f\x9b\xa0  ") + typeAssist, card);
            assistLabel->setStyleSheet("QLabel{font-size:11px;color:#00b894;background:#e8f8f5;"
                "border-radius:8px;padding:4px 10px;border:none;}");
            assistLabel->setWordWrap(true);
            equipRow->addWidget(assistLabel, 1);

            QLabel *sensorLabel = new QLabel(
                QString::fromUtf8("\xf0\x9f\x93\xa1 %1 capteurs").arg(nbSensors), card);
            sensorLabel->setStyleSheet("QLabel{font-size:11px;color:#e17055;background:#fef3e2;"
                "border-radius:8px;padding:4px 10px;border:none;font-weight:bold;}");
            equipRow->addWidget(sensorLabel);

            cardL->addLayout(equipRow);

            // Sensor description
            if(!descSensors.isEmpty()){
                QLabel *descL = new QLabel(QString::fromUtf8("\xf0\x9f\x94\xa7  ") + descSensors, card);
                descL->setStyleSheet("QLabel{font-size:10px;color:#b2bec3;background:transparent;border:none;}");
                cardL->addWidget(descL);
            }

            // Port info
            QLabel *portLabel = new QLabel(QString::fromUtf8("\xf0\x9f\x94\x8c Port: ") + port, card);
            portLabel->setStyleSheet("QLabel{font-size:10px;color:#b2bec3;background:transparent;border:none;}");
            cardL->addWidget(portLabel);

            // Tarif + Select in one row
            QHBoxLayout *bottomRow = new QHBoxLayout();
            double tarif = v.contains("tarif_seance") ? v["tarif_seance"].toDouble() : 15.0;
            QLabel *tarifLabel = new QLabel(
                QString::fromUtf8("💰  %1 DT / session").arg(tarif, 0, 'f', 1), card);
            tarifLabel->setStyleSheet("QLabel{font-size:13px;font-weight:bold;color:#2d3436;"
                "background:transparent;border:none;}");
            bottomRow->addWidget(tarifLabel);
            bottomRow->addStretch();

            // Select Button
            QPushButton *selectBtn = new QPushButton(
                QString::fromUtf8("Select  →"), card);
            selectBtn->setFixedHeight(44);
            selectBtn->setMinimumWidth(180);
            selectBtn->setCursor(Qt::PointingHandCursor);
            selectBtn->setStyleSheet(
                "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                "stop:0 #00b894,stop:1 #55efc4);color:white;border:none;"
                "border-radius:12px;font-size:13px;font-weight:bold;letter-spacing:0.5px;}"
                "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                "stop:0 #009874,stop:1 #00b894);}");
            connect(selectBtn, &QPushButton::clicked, this,
                    [this, vId](){ onVehicleSelected(vId); });
            bottomRow->addWidget(selectBtn);
            cardL->addLayout(bottomRow);

            vehGrid->addWidget(card, idx / 2, idx % 2);
        }
    }

    cL->addLayout(vehGrid);

    // ── Bottom info ──
    QFrame *infoCard = new QFrame(this);
    infoCard->setObjectName("vehInfoCard");
    infoCard->setStyleSheet(cardSS("vehInfoCard"));
    infoCard->setGraphicsEffect(shadow(infoCard, 10, 8));
    QVBoxLayout *infoL = new QVBoxLayout(infoCard);
    infoL->setContentsMargins(18,14,18,14);
    infoL->setSpacing(8);

    QLabel *infoTitle = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x92\xa1 Smart autonomous training"), this);
    infoTitle->setStyleSheet("QLabel{font-size:14px;font-weight:bold;color:#00b894;background:transparent;border:none;}");
    infoL->addWidget(infoTitle);

    QLabel *infoDesc = new QLabel(QString::fromUtf8(
        "The vehicles are equipped with ultrasonic sensors and a guidance application.\n"
        "The student can practice the ATTT exam maneuvers independently:\n"
        "\xe2\x80\xa2 Step-by-step guidance with voice and visual instructions\n"
        "\xe2\x80\xa2 Real-time obstacle detection\n"
        "\xe2\x80\xa2 Automatic scoring and progress history\n"
        "\xe2\x80\xa2 Exam mode with timer and ATTT criteria"), this);
    infoDesc->setStyleSheet("QLabel{font-size:11px;color:#636e72;background:transparent;border:none;line-height:1.5;}");
    infoDesc->setWordWrap(true);
    infoL->addWidget(infoDesc);
    cL->addWidget(infoCard);

    cL->addStretch();
    scroll->setWidget(content);
    pL->addWidget(scroll, 1);

    // ── Stats FAB (floating bubble — bottom-right of vehicle selection page) ──
    m_vehStatsFAB = new QPushButton(QString::fromUtf8("\xf0\x9f\x93\x8a"), page);
    m_vehStatsFAB->setFixedSize(56, 56);
    m_vehStatsFAB->setCursor(Qt::PointingHandCursor);
    m_vehStatsFAB->setToolTip(QString::fromUtf8("Statistics & Progress"));
    m_vehStatsFAB->setStyleSheet(
        "QPushButton{"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #6c5ce7, stop:1 #5a4bd1);"
        "  color: white; font-size: 24px; border: none; border-radius: 28px;"
        "  box-shadow: 0 4px 15px rgba(108,92,231,0.4);"
        "}"
        "QPushButton:hover{ background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #7d6ff0, stop:1 #6c5ce7); }"
        "QPushButton:pressed{ background: #4a3bc0; }"
    );
    m_vehStatsFAB->setGraphicsEffect(shadow(m_vehStatsFAB, 20, 40));
    connect(m_vehStatsFAB, &QPushButton::clicked, this, [this](){
        QDialog *dlg = new QDialog(this);
        dlg->setWindowTitle(QString::fromUtf8("Statistics & Progress — %1").arg(m_userName));
        dlg->setWindowFlags(dlg->windowFlags() | Qt::Window);
        dlg->resize(900, 680);
        dlg->setAttribute(Qt::WA_DeleteOnClose);

        QVBoxLayout *dlgL = new QVBoxLayout(dlg);
        dlgL->setContentsMargins(0, 0, 0, 0);
        dlgL->setSpacing(0);

        StatisticsWidget *sw = new StatisticsWidget(m_userName, m_userRole,
                                                     m_currentEleveId, dlg);
        dlgL->addWidget(sw, 1);

        // Close button bar
        QFrame *bar = new QFrame(dlg);
        bar->setFixedHeight(48);
        bar->setStyleSheet("QFrame{background:#f0f2f5;border-top:1px solid #e2e8f0;}");
        QHBoxLayout *barL = new QHBoxLayout(bar);
        barL->setContentsMargins(16,0,16,0);
        barL->addStretch();
        QPushButton *closeBtn = new QPushButton(QString::fromUtf8("Close"), bar);
        closeBtn->setFixedSize(100, 32);
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setStyleSheet(
            "QPushButton{background:#6c5ce7;color:white;border:none;border-radius:8px;"
            "font-size:12px;font-weight:bold;}"
            "QPushButton:hover{background:#5a4bd1;}");
        connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
        barL->addWidget(closeBtn);
        dlgL->addWidget(bar);

        dlg->exec();
    });
    m_vehStatsFAB->raise();
    m_vehPage = page;

    return page;
}

// ═══════════════════════════════════════════════════════════════════
//  HOME PAGE — Maneuver picker + Stats (replaces dashboard + vehicle)
// ═══════════════════════════════════════════════════════════════════

QWidget* ParkingWidget::createHomePage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName("homePg");
    page->setStyleSheet(QString("QWidget#homePg{%1}").arg(BG));

    QVBoxLayout *pL = new QVBoxLayout(page);
    pL->setContentsMargins(0,0,0,0);
    pL->setSpacing(0);

    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(SCROLL_SS);

    QWidget *content = new QWidget(this);
    content->setObjectName("homeContent");
    content->setStyleSheet("QWidget#homeContent{background:#f0f2f5;}");
    QVBoxLayout *cL = new QVBoxLayout(content);
    cL->setContentsMargins(28,20,28,20);
    cL->setSpacing(16);

    // ══════════════════════════════════════════
    //  HERO WELCOME CARD — gradient accent strip
    // ══════════════════════════════════════════
    QFrame *hero = new QFrame(this);
    hero->setObjectName("heroW");
    hero->setStyleSheet("QFrame#heroW{background:white;border-radius:16px;"
        "border:1px solid #e2e8f0;}");
    hero->setGraphicsEffect(shadow(hero, 16, 10));
    hero->setFixedHeight(72);
    QHBoxLayout *heroL = new QHBoxLayout(hero);
    heroL->setContentsMargins(20,0,16,0);
    heroL->setSpacing(14);

    // Green accent dot
    QLabel *accentDot = new QLabel("", this);
    accentDot->setFixedSize(6, 36);
    accentDot->setStyleSheet("QLabel{background:#00b894;border-radius:3px;border:none;}");
    heroL->addWidget(accentDot);

    QVBoxLayout *heroText = new QVBoxLayout();
    heroText->setSpacing(3);
    QLabel *hi = new QLabel(QString::fromUtf8("Choose a maneuver"), this);
    hi->setStyleSheet("QLabel{font-size:16px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    heroText->addWidget(hi);
    QLabel *subHi = new QLabel(QString::fromUtf8(
        "Hello %1 \xe2\x80\x94 Practice the 5 ATTT exam maneuvers with real-time guidance").arg(m_userName), this);
    subHi->setStyleSheet("QLabel{font-size:10px;color:#636e72;background:transparent;border:none;}");
    heroText->addWidget(subHi);
    heroL->addLayout(heroText, 1);

    QPushButton *changeCarBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x84 Change Vehicle"), this);
    changeCarBtn->setCursor(Qt::PointingHandCursor);
    changeCarBtn->setFixedHeight(32);
    changeCarBtn->setStyleSheet("QPushButton{background:#f0f2f5;color:#636e72;border:none;"
        "border-radius:10px;padding:0 14px;font-size:10px;font-weight:bold;}"
        "QPushButton:hover{background:#e2e8f0;}");
    connect(changeCarBtn, &QPushButton::clicked, this, [this](){ m_stack->setCurrentIndex(0); });
    heroL->addWidget(changeCarBtn);

    QPushButton *coachBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\xa7\xa0 Coach & Stats"), this);
    coachBtn->setCursor(Qt::PointingHandCursor);
    coachBtn->setFixedHeight(32);
    coachBtn->setStyleSheet("QPushButton{background:#6c5ce7;color:white;border:none;"
        "border-radius:10px;padding:0 16px;font-size:10px;font-weight:bold;}"
        "QPushButton:hover{background:#5a4bd1;}");
    connect(coachBtn, &QPushButton::clicked, this, [this](){ m_stack->setCurrentIndex(4); });
    heroL->addWidget(coachBtn);

    cL->addWidget(hero);

    // ══════════════════════════════════════════
    //  4 STAT CARDS ROW
    // ══════════════════════════════════════════
    QHBoxLayout *statsRow = new QHBoxLayout();
    statsRow->setSpacing(12);

    auto makeStatCard = [&](const QString &icon, const QString &def,
                            const QString &label, const QString &accent, QLabel **ref) {
        QFrame *sc = new QFrame(this);
        static int scid = 0;
        QString oid = QString("hsc%1").arg(scid++);
        sc->setObjectName(oid);
        sc->setStyleSheet(QString("QFrame#%1{background:white;border-radius:14px;"
            "border:1px solid #e2e8f0;}").arg(oid));
        QVBoxLayout *sl = new QVBoxLayout(sc);
        sl->setContentsMargins(14,10,14,10);
        sl->setSpacing(4);
        QLabel *ic = new QLabel(icon, sc);
        ic->setStyleSheet("QLabel{font-size:18px;background:transparent;border:none;}");
        sl->addWidget(ic);
        QLabel *v = new QLabel(def, sc);
        v->setStyleSheet(QString("QLabel{font-size:18px;font-weight:bold;color:%1;"
            "background:transparent;border:none;}").arg(accent));
        sl->addWidget(v);
        *ref = v;
        QLabel *lb = new QLabel(label, sc);
        lb->setStyleSheet("QLabel{font-size:9px;color:#b2bec3;font-weight:600;"
            "background:transparent;border:none;}");
        sl->addWidget(lb);
        return sc;
    };

    statsRow->addWidget(makeStatCard(QString::fromUtf8("\xf0\x9f\x8e\xaf"), "0", "Sessions", "#00b894", &m_statSessions));
    statsRow->addWidget(makeStatCard(QString::fromUtf8("\xe2\x9c\x85"), "0%", "Success", "#6c5ce7", &m_statSuccess));
    statsRow->addWidget(makeStatCard(QString::fromUtf8("\xe2\x8f\xb1"), "--:--", "Best Time", "#0984e3", &m_statBest));
    statsRow->addWidget(makeStatCard(QString::fromUtf8("\xf0\x9f\x94\xa5"), "0", "Streak", "#e17055", &m_statStreak));
    cL->addLayout(statsRow);

    // ══════════════════════════════════════════
    //  MANEUVER GRID (2 columns)
    // ══════════════════════════════════════════
    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(14);
    m_masteryBars.clear();
    m_masteryPcts.clear();
    for(int i = 0; i < 5; i++){
        grid->addWidget(createManeuverCard(i), i / 2, i % 2);
    }
    cL->addLayout(grid);

    // ══════════════════════════════════════════
    //  FULL EXAM CARD
    // ══════════════════════════════════════════
    cL->addWidget(createFullExamCard());

    // ══════════════════════════════════════════
    //  SMART RECOMMENDATION CARD
    // ══════════════════════════════════════════
    cL->addWidget(createSmartRecommendationCard());

    // ══════════════════════════════════════════
    //  WEATHER CONDITIONS
    // ══════════════════════════════════════════
    cL->addWidget(createWeatherCard());

    // ══════════════════════════════════════════
    //  VIDEO TUTORIALS ROW
    // ══════════════════════════════════════════
    QLabel *vidTitle = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x8e\xac  Video Tutorials — Learn from real ATTT exams"), this);
    vidTitle->setStyleSheet("QLabel{font-size:14px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    cL->addWidget(vidTitle);

    for (int i = 0; i < 5; i++) {
        cL->addWidget(createVideoTutorialCard(i));
    }

    // ══════════════════════════════════════════
    //  PERFORMANCE TREND
    // ══════════════════════════════════════════
    cL->addWidget(createPerformanceTrendCard());

    // ══════════════════════════════════════════
    //  EXAM READINESS + HISTORY + BADGES
    // ══════════════════════════════════════════
    cL->addWidget(createExamReadinessCard());

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(14);
    bottomRow->addWidget(createRadarChart(), 1);
    QVBoxLayout *rightBottomCol = new QVBoxLayout();
    rightBottomCol->setSpacing(14);
    rightBottomCol->addWidget(createAchievementsStrip());
    rightBottomCol->addWidget(createHistoryCard());
    bottomRow->addLayout(rightBottomCol, 1);
    cL->addLayout(bottomRow);

    cL->addStretch();
    scroll->setWidget(content);
    pL->addWidget(scroll, 1);
    return page;
}

QWidget* ParkingWidget::createStatPill(const QString &icon, const QString &val,
    const QString &label, const QString &grad)
{
    QFrame *c = new QFrame(this);
    QString sid = QString("stP_%1").arg(label.left(4));
    c->setObjectName(sid);
    c->setStyleSheet(QString("QFrame#%1{background:white;border-radius:14px;border:1px solid #e2e8f0;}").arg(sid));
    c->setFixedHeight(78);
    c->setGraphicsEffect(shadow(c, 14, 8));
    QHBoxLayout *l = new QHBoxLayout(c);
    l->setContentsMargins(16,0,16,0);
    l->setSpacing(12);
    QLabel *ic = new QLabel(icon, c);
    ic->setFixedSize(42,42);
    ic->setAlignment(Qt::AlignCenter);
    ic->setStyleSheet(QString(
        "QLabel{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,%1);"
        "border-radius:12px;font-size:18px;border:none;color:white;}")
        .arg(grad.isEmpty() ? "stop:0 #00b894,stop:1 #55efc4" : grad));
    l->addWidget(ic);
    QVBoxLayout *t = new QVBoxLayout();
    t->setSpacing(2);
    QLabel *v = new QLabel(val, c);
    v->setStyleSheet("QLabel{font-size:20px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    t->addWidget(v);
    QLabel *lb = new QLabel(label, c);
    lb->setStyleSheet("QLabel{font-size:9px;color:#b2bec3;font-weight:600;"
        "letter-spacing:0.5px;background:transparent;border:none;}");
    t->addWidget(lb);
    l->addLayout(t, 1);
    return c;
}

QWidget* ParkingWidget::createManeuverCard(int index)
{
    QString name = m_maneuverNames[index];
    QString icon = m_maneuverIcons[index];
    QString color = m_maneuverColors[index];
    int steps = m_allManeuvers[index].size();

    QStringList diffs = {"Hard", "Medium", "Easy", "Medium", "Medium"};
    QStringList diffCols = {"#e17055", "#fdcb6e", "#00b894", "#fdcb6e", "#fdcb6e"};
    QStringList diffBgs = {"#fef3e2", "#fef9e7", "#e8f8f5", "#fef9e7", "#fef9e7"};
    QStringList descs = {
        QString::fromUtf8("Parallel parking"),
        QString::fromUtf8("Perpendicular parking"),
        QString::fromUtf8("Diagonal parking"),
        QString::fromUtf8("Reverse in straight line"),
        QString::fromUtf8("3-point turn")
    };
    QStringList grads = {
        "stop:0 #00b894,stop:1 #55efc4",
        "stop:0 #0984e3,stop:1 #74b9ff",
        "stop:0 #6c5ce7,stop:1 #a29bfe",
        "stop:0 #e17055,stop:1 #fab1a0",
        "stop:0 #fdcb6e,stop:1 #ffeaa7"
    };

    QFrame *card = new QFrame(this);
    QString cid = QString("mc%1").arg(index);
    card->setObjectName(cid);
    card->setStyleSheet(QString(
        "QFrame#%1{background:white;border-radius:18px;border:1px solid #e2e8f0;}"
        "QFrame#%1:hover{border-color:%2;}").arg(cid, color));
    card->setGraphicsEffect(shadow(card, 16, 10));
    card->setMinimumHeight(190);
    card->setCursor(Qt::PointingHandCursor);

    QVBoxLayout *l = new QVBoxLayout(card);
    l->setContentsMargins(20,18,20,16);
    l->setSpacing(12);

    // Top: gradient icon + name + difficulty badge
    QHBoxLayout *topRow = new QHBoxLayout();
    topRow->setSpacing(14);

    QLabel *ic = new QLabel(icon, card);
    ic->setFixedSize(50,50);
    ic->setAlignment(Qt::AlignCenter);
    ic->setStyleSheet(QString(
        "QLabel{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,%1);"
        "border-radius:15px;font-size:22px;border:none;color:white;}").arg(grads[index]));
    topRow->addWidget(ic);

    QVBoxLayout *nameCol = new QVBoxLayout();
    nameCol->setSpacing(3);
    QLabel *nm = new QLabel(name, card);
    nm->setStyleSheet("QLabel{font-size:15px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    nameCol->addWidget(nm);
    QLabel *desc = new QLabel(descs[index], card);
    desc->setStyleSheet("QLabel{font-size:10px;color:#b2bec3;background:transparent;border:none;}");
    nameCol->addWidget(desc);
    topRow->addLayout(nameCol, 1);

    QLabel *diff = new QLabel(diffs[index], card);
    diff->setStyleSheet(QString(
        "QLabel{font-size:9px;font-weight:bold;color:%1;"
        "background:%2;border-radius:10px;padding:4px 12px;border:none;}")
        .arg(diffCols[index], diffBgs[index]));
    topRow->addWidget(diff, 0, Qt::AlignTop);
    l->addLayout(topRow);

    QLabel *stepsInfo = new QLabel(QString::fromUtf8(
        "%1 steps  \xc2\xb7  ~%2 min").arg(steps).arg(steps * 2), card);
    stepsInfo->setStyleSheet("QLabel{font-size:10px;color:#b2bec3;"
        "background:transparent;border:none;}");
    l->addWidget(stepsInfo);

    // Gradient mastery bar
    QHBoxLayout *barRow = new QHBoxLayout();
    barRow->setSpacing(10);
    QLabel *barLbl = new QLabel(QString::fromUtf8("Mastery"), card);
    barLbl->setStyleSheet("QLabel{font-size:9px;color:#b2bec3;font-weight:600;"
        "background:transparent;border:none;}");
    barRow->addWidget(barLbl);

    QProgressBar *bar = new QProgressBar(card);
    bar->setRange(0,100); bar->setValue(0);
    bar->setFixedHeight(7); bar->setTextVisible(false);
    bar->setStyleSheet(QString(
        "QProgressBar{background:#f0f2f5;border:none;border-radius:3px;}"
        "QProgressBar::chunk{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,%1);"
        "border-radius:3px;}").arg(grads[index]));
    barRow->addWidget(bar, 1);

    QLabel *pct = new QLabel("0%", card);
    pct->setStyleSheet(QString("QLabel{font-size:10px;font-weight:bold;color:%1;"
        "background:transparent;border:none;}").arg(color));
    barRow->addWidget(pct);
    m_masteryBars.append(bar);
    m_masteryPcts.append(pct);
    l->addLayout(barRow);

    // Gradient button
    QPushButton *go = new QPushButton(QString::fromUtf8("Start  \xe2\x86\x92"), card);
    go->setCursor(Qt::PointingHandCursor);
    go->setFixedHeight(38);
    go->setStyleSheet(QString(
        "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,%1);"
        "color:white;border:none;border-radius:12px;"
        "padding:0 24px;font-size:12px;font-weight:bold;letter-spacing:0.5px;}"
        "QPushButton:hover{background:%2;}").arg(grads[index], color));
    connect(go, &QPushButton::clicked, this, [this, index](){ openManeuver(index); });

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(go);
    l->addLayout(btnRow);

    return card;
}

QWidget* ParkingWidget::createRadarChart()
{
    QFrame *c = new QFrame(this);
    c->setObjectName("radarC");
    c->setStyleSheet(cardSS("radarC"));
    c->setGraphicsEffect(shadow(c));
    c->setMinimumHeight(200);
    QVBoxLayout *l = new QVBoxLayout(c);
    l->setContentsMargins(18,14,18,14);
    l->setSpacing(8);

    QLabel *t = new QLabel(QString::fromUtf8("\xf0\x9f\x93\x8a  Maneuver progress"), c);
    t->setStyleSheet("QLabel{font-size:14px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
    l->addWidget(t);

    // Show 5 mini progress rows — one per maneuver
    for(int i = 0; i < 5 && i < m_maneuverNames.size(); i++){
        QWidget *row = new QWidget(c);
        row->setStyleSheet("background:transparent;");
        QHBoxLayout *rl = new QHBoxLayout(row);
        rl->setContentsMargins(0,2,0,2);
        rl->setSpacing(8);

        QLabel *icon = new QLabel(m_maneuverIcons[i], row);
        icon->setFixedWidth(24);
        icon->setStyleSheet("QLabel{font-size:14px;background:transparent;border:none;}");
        rl->addWidget(icon);

        QLabel *name = new QLabel(m_maneuverNames[i], row);
        name->setFixedWidth(90);
        name->setStyleSheet("QLabel{font-size:10px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
        rl->addWidget(name);

        QProgressBar *bar = new QProgressBar(row);
        bar->setRange(0,100);
        int pct = m_maneuverAttemptCounts[i] > 0 ?
            (m_maneuverSuccessCounts[i]*100/m_maneuverAttemptCounts[i]) : 0;
        bar->setValue(pct);
        bar->setFixedHeight(10);
        bar->setTextVisible(false);
        bar->setStyleSheet(QString(
            "QProgressBar{background:#f0f2f5;border:none;border-radius:5px;}"
            "QProgressBar::chunk{background:%1;border-radius:5px;}").arg(m_maneuverColors[i]));
        rl->addWidget(bar, 1);

        QLabel *pctLabel = new QLabel(QString("%1%").arg(pct), row);
        pctLabel->setFixedWidth(30);
        pctLabel->setStyleSheet(QString("QLabel{font-size:10px;font-weight:bold;color:%1;"
            "background:transparent;border:none;}").arg(m_maneuverColors[i]));
        rl->addWidget(pctLabel);

        QLabel *countLabel = new QLabel(QString("(%1/%2)")
            .arg(m_maneuverSuccessCounts[i]).arg(m_maneuverAttemptCounts[i]), row);
        countLabel->setFixedWidth(40);
        countLabel->setStyleSheet("QLabel{font-size:8px;color:#b2bec3;background:transparent;border:none;}");
        rl->addWidget(countLabel);

        l->addWidget(row);
    }

    // Total summary
    int totalRate = m_totalSessionsCount > 0 ?
        (m_totalSuccessCount * 100 / m_totalSessionsCount) : 0;
    QLabel *summary = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x93\x88  Overall: %1% success rate over %2 sessions")
        .arg(totalRate).arg(m_totalSessionsCount), c);
    summary->setStyleSheet("QLabel{font-size:10px;color:#636e72;background:#f8f8f8;"
        "border-radius:8px;padding:6px 10px;border:none;}");
    l->addWidget(summary);

    return c;
}

QWidget* ParkingWidget::createAchievementsStrip()
{
    QFrame *c = new QFrame(this);
    c->setObjectName("achC");
    c->setStyleSheet(cardSS("achC"));
    c->setGraphicsEffect(shadow(c));
    QVBoxLayout *l = new QVBoxLayout(c);
    l->setContentsMargins(18,14,18,14);
    l->setSpacing(10);
    QLabel *t = new QLabel(QString::fromUtf8("\xf0\x9f\x8f\x86  Badges"), c);
    t->setStyleSheet("QLabel{font-size:14px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
    l->addWidget(t);

    QGridLayout *g = new QGridLayout();
    g->setSpacing(8);
    struct B { QString ic; QString nm; };
    QList<B> badges = {
        {QString::fromUtf8("\xf0\x9f\x8c\xb1"), "1er pas"},
        {QString::fromUtf8("\xf0\x9f\x94\xb2"), QString::fromUtf8("Parallel")},
        {QString::fromUtf8("\xe2\x9a\xa1"), "Rapide"},
        {QString::fromUtf8("\xf0\x9f\x94\xa5"), "3 jours"},
        {QString::fromUtf8("\xf0\x9f\x8e\xaf"), "Parfait"},
        {QString::fromUtf8("\xf0\x9f\x91\x91"), "Master"}
    };
    m_achieveIcons.clear();
    for(int i = 0; i < badges.size(); i++){
        QWidget *bw = new QWidget(c);
        bw->setStyleSheet("background:transparent;");
        QVBoxLayout *bl = new QVBoxLayout(bw);
        bl->setContentsMargins(4,6,4,6);
        bl->setSpacing(3);
        bl->setAlignment(Qt::AlignCenter);
        QLabel *ic = new QLabel(QString::fromUtf8("\xf0\x9f\x94\x92"), bw);
        ic->setAlignment(Qt::AlignCenter);
        ic->setFixedSize(36,36);
        ic->setStyleSheet("QLabel{font-size:18px;background:#f8f8f8;border-radius:18px;border:none;}");
        bl->addWidget(ic, 0, Qt::AlignCenter);
        m_achieveIcons.append(ic);
        QLabel *nm = new QLabel(badges[i].nm, bw);
        nm->setAlignment(Qt::AlignCenter);
        nm->setStyleSheet("QLabel{font-size:9px;color:#b2bec3;background:transparent;border:none;}");
        bl->addWidget(nm);
        g->addWidget(bw, i/3, i%3);
    }
    l->addLayout(g);
    return c;
}

QWidget* ParkingWidget::createHistoryCard()
{
    QFrame *c = new QFrame(this);
    c->setObjectName("hstC");
    c->setStyleSheet(cardSS("hstC"));
    c->setGraphicsEffect(shadow(c));
    QVBoxLayout *l = new QVBoxLayout(c);
    l->setContentsMargins(18,14,18,14);
    l->setSpacing(8);

    QHBoxLayout *hdrRow = new QHBoxLayout();
    QLabel *t = new QLabel(QString::fromUtf8("\xf0\x9f\x93\x9c  Historique"), c);
    t->setStyleSheet("QLabel{font-size:14px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
    hdrRow->addWidget(t);
    hdrRow->addStretch();

    // Refresh button
    QPushButton *refreshBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x84"), c);
    refreshBtn->setFixedSize(24,24);
    refreshBtn->setCursor(Qt::PointingHandCursor);
    refreshBtn->setStyleSheet("QPushButton{background:#f0f2f5;border:none;border-radius:12px;font-size:11px;}"
        "QPushButton:hover{background:#d1f2eb;}");
    connect(refreshBtn, &QPushButton::clicked, this, &ParkingWidget::refreshHistoryUI);
    hdrRow->addWidget(refreshBtn);
    l->addLayout(hdrRow);

    m_historyLayout = new QVBoxLayout();
    m_historyLayout->setSpacing(4);

    // Will be populated by refreshHistoryUI()
    QLabel *empty = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x93\xad  No sessions yet\nStart your first practice!"), c);
    empty->setAlignment(Qt::AlignCenter);
    empty->setStyleSheet("QLabel{font-size:11px;color:#b2bec3;background:#f8f8f8;border-radius:10px;padding:16px;border:none;}");
    m_historyLayout->addWidget(empty);

    l->addLayout(m_historyLayout);
    return c;
}


// ═══════════════════════════════════════════════════════════════════
//  PARKING DIAGRAM WIDGET — 2D Bird's-Eye View for Créneau
// ═══════════════════════════════════════════════════════════════════


// ═══════════════════════════════════════════════════════════════════
//  SESSION PAGE
// ═══════════════════════════════════════════════════════════════════

QWidget* ParkingWidget::createSessionPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName("sessPg");
    page->setStyleSheet(QString("QWidget#sessPg{%1}").arg(BG));
    QVBoxLayout *pL = new QVBoxLayout(page);
    pL->setContentsMargins(0,0,0,0);
    pL->setSpacing(0);

    // ══════════════════════════════════════════
    //  TOP BAR (fixed)
    // ══════════════════════════════════════════
    QWidget *topBar = new QWidget(this);
    topBar->setObjectName("sTB");
    topBar->setStyleSheet("QWidget#sTB{background:white;border-bottom:1px solid #e2e8f0;}");
    topBar->setFixedHeight(50);
    QHBoxLayout *tbL = new QHBoxLayout(topBar);
    tbL->setContentsMargins(14,0,14,0);
    tbL->setSpacing(8);

    QPushButton *back = new QPushButton(QString::fromUtf8("\xe2\x86\x90"), topBar);
    back->setFixedSize(32,32);
    back->setCursor(Qt::PointingHandCursor);
    back->setStyleSheet("QPushButton{background:#f0f2f5;color:#2d3436;border:none;"
        "border-radius:16px;font-size:14px;font-weight:bold;}"
        "QPushButton:hover{background:#e2e8f0;}");
    connect(back, &QPushButton::clicked, this, [this](){
        if(m_sessionActive){
            QMessageBox::warning(this, "", "Finish the session first.");
            return;
        }
        m_stack->setCurrentIndex(1);
    });
    tbL->addWidget(back);

    m_sessionTitle = new QLabel("", topBar);
    m_sessionTitle->setStyleSheet("QLabel{font-size:13px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    tbL->addWidget(m_sessionTitle);
    tbL->addStretch();

    m_timerLabel = new QLabel("00:00", topBar);
    m_timerLabel->setStyleSheet("QLabel{font-size:26px;font-weight:bold;color:#2d3436;"
        "font-family:'Consolas','Courier New',monospace;background:transparent;border:none;}");
    tbL->addWidget(m_timerLabel);

    m_examTimerLabel = new QLabel("", topBar);
    m_examTimerLabel->setStyleSheet("QLabel{font-size:12px;font-weight:bold;color:#e17055;"
        "font-family:'Courier New',monospace;background:#fef3e2;"
        "border-radius:8px;padding:3px 10px;border:none;}");
    m_examTimerLabel->setVisible(false);
    tbL->addWidget(m_examTimerLabel);
    tbL->addStretch();

    QString pillOff = "QPushButton{background:#f5f6fa;color:#636e72;border:none;border-radius:12px;"
        "padding:5px 12px;font-size:9px;font-weight:bold;}QPushButton:hover{background:#e8eaed;}";

    m_examToggle = new QPushButton(QString::fromUtf8("\xf0\x9f\x8e\x93 Exam"), topBar);
    m_examToggle->setCheckable(true); m_examToggle->setCursor(Qt::PointingHandCursor);
    m_examToggle->setFixedHeight(26);
    m_examToggle->setStyleSheet(pillOff + "QPushButton:checked{background:#e17055;color:white;}");
    connect(m_examToggle, &QPushButton::toggled, this, [this](bool c){ m_examMode = c; });
    tbL->addWidget(m_examToggle);

    m_freeToggle = new QPushButton(QString::fromUtf8("\xf0\x9f\x8e\xaf Free"), topBar);
    m_freeToggle->setCheckable(true); m_freeToggle->setCursor(Qt::PointingHandCursor);
    m_freeToggle->setFixedHeight(26);
    m_freeToggle->setStyleSheet(pillOff + "QPushButton:checked{background:#00b894;color:white;}");
    connect(m_freeToggle, &QPushButton::toggled, this, [this](bool c){ m_freeTrainingMode = c; });
    tbL->addWidget(m_freeToggle);

    m_simToggle = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x84 Sim"), topBar);
    m_simToggle->setCheckable(true); m_simToggle->setCursor(Qt::PointingHandCursor);
    m_simToggle->setFixedHeight(26);
    m_simToggle->setStyleSheet(pillOff + "QPushButton:checked{background:#0984e3;color:white;}");
    connect(m_simToggle, &QPushButton::toggled, this, [this](bool c){
        m_simulationMode = c;
        m_sensorMgr->enableSimulation(c);
        if (m_simButtonPanel) m_simButtonPanel->setVisible(c);
    });
    tbL->addWidget(m_simToggle);

    m_voiceToggle = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x8a"), topBar);
    m_voiceToggle->setCheckable(true); m_voiceToggle->setChecked(true);
    m_voiceToggle->setCursor(Qt::PointingHandCursor);
    m_voiceToggle->setFixedSize(26,26);
    m_voiceToggle->setStyleSheet(pillOff + "QPushButton:checked{background:#6c5ce7;color:white;}");
    connect(m_voiceToggle, &QPushButton::toggled, this, [this](bool c){
        m_voiceEnabled = c;
        if(c) speak("Voice assistance enabled.");
    });
    tbL->addWidget(m_voiceToggle);
    pL->addWidget(topBar);

    // ══════════════════════════════════════════
    //  SCROLLABLE CONTENT — Single column, top to bottom
    // ══════════════════════════════════════════
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(SCROLL_SS);

    QWidget *scrollContent = new QWidget(this);
    scrollContent->setObjectName("sessScroll");
    scrollContent->setStyleSheet("QWidget#sessScroll{background:#f0f2f5;}");
    QVBoxLayout *sL = new QVBoxLayout(scrollContent);
    sL->setContentsMargins(20,14,20,14);
    sL->setSpacing(10);

    // ────────────────────────────────────
    //  SECTION 1: Step Guide Card
    // ────────────────────────────────────
    QFrame *stepCard = new QFrame(this);
    stepCard->setObjectName("stpC");
    stepCard->setStyleSheet("QFrame#stpC{background:white;border-radius:14px;border:1px solid #e2e8f0;}");
    stepCard->setMinimumHeight(420);
    stepCard->setGraphicsEffect(shadow(stepCard));
    QVBoxLayout *stL = new QVBoxLayout(stepCard);
    stL->setContentsMargins(18,14,18,14);
    stL->setSpacing(8);

    // Dots row
    QHBoxLayout *dotsRow = new QHBoxLayout();
    m_dotsLayout = new QHBoxLayout();
    m_dotsLayout->setSpacing(4);
    m_dotsLayout->setAlignment(Qt::AlignLeft);
    dotsRow->addLayout(m_dotsLayout);
    dotsRow->addStretch();
    stL->addLayout(dotsRow);

    // Step number + title + instruction
    QHBoxLayout *stepRow = new QHBoxLayout();
    stepRow->setSpacing(14);

    m_stepNum = new QLabel("1", stepCard);
    m_stepNum->setFixedSize(44,44);
    m_stepNum->setAlignment(Qt::AlignCenter);
    m_stepNum->setStyleSheet("QLabel{background:#00b894;color:white;font-size:18px;"
        "font-weight:bold;border-radius:22px;border:none;}");
    stepRow->addWidget(m_stepNum, 0, Qt::AlignTop);

    QVBoxLayout *instrCol = new QVBoxLayout();
    instrCol->setSpacing(4);
    m_stepTitle = new QLabel("Select a maneuver", stepCard);
    m_stepTitle->setStyleSheet("QLabel{font-size:15px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    m_stepTitle->setWordWrap(true);
    instrCol->addWidget(m_stepTitle);
    m_stepInstruction = new QLabel("Choose from the home page.", stepCard);
    m_stepInstruction->setStyleSheet("QLabel{font-size:11px;color:#636e72;"
        "background:transparent;border:none;}");
    m_stepInstruction->setWordWrap(true);
    instrCol->addWidget(m_stepInstruction);
    stepRow->addLayout(instrCol, 1);
    stL->addLayout(stepRow);

    // Bird's-eye step diagram (QPainter)
    m_stepDiagram = new StepDiagramWidget(stepCard);
    m_stepDiagram->setMinimumHeight(180);
    stL->addWidget(m_stepDiagram);

    // Tip
    m_stepTip = new QLabel("", stepCard);
    m_stepTip->setWordWrap(true);
    m_stepTip->setStyleSheet("QLabel{font-size:10px;color:#00b894;background:#f0faf7;"
        "border-radius:8px;padding:8px 12px;border:none;}");
    stL->addWidget(m_stepTip);

    // Sensor
    m_stepSensor = new QLabel("", stepCard);
    m_stepSensor->setWordWrap(true);
    m_stepSensor->setStyleSheet("QLabel{font-size:10px;color:#0984e3;background:#eef6fd;"
        "border-radius:8px;padding:6px 12px;border:none;}");
    stL->addWidget(m_stepSensor);

    // DB sensor message (from PARKING_MANEUVER_STEPS.guidance_message)
    m_dbMsgLabel = new QLabel("", stepCard);
    m_dbMsgLabel->setWordWrap(true);
    m_dbMsgLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_dbMsgLabel->setStyleSheet(
        "QLabel{background:#fff3cd;color:#856404;font-size:12px;font-weight:bold;"
        "border-radius:10px;padding:10px 14px;"
        "border-left:4px solid #ffc107;border-top:none;border-right:none;border-bottom:none;}");
    m_dbMsgLabel->setVisible(false);
    stL->addWidget(m_dbMsgLabel);

    // Guidance
    m_guidanceMsg = new QLabel("", stepCard);
    m_guidanceMsg->setWordWrap(true);
    m_guidanceMsg->setStyleSheet("QLabel{font-size:11px;color:#00b894;font-weight:bold;"
        "background:#e8f8f5;border-radius:8px;padding:8px 12px;border:none;}");
    m_guidanceMsg->setVisible(false);
    stL->addWidget(m_guidanceMsg);

    stL->addStretch();

    // ── Common Mistake (inside step card) ──
    QFrame *mstSep = new QFrame(stepCard);
    mstSep->setFrameShape(QFrame::HLine);
    mstSep->setStyleSheet("QFrame{background:#fdebd0;max-height:1px;border:none;}");
    stL->addWidget(mstSep);

    QLabel *mstH = new QLabel(QString::fromUtf8("\xe2\x9a\xa0 Common Mistake"), stepCard);
    mstH->setStyleSheet("QLabel{font-size:10px;font-weight:bold;color:#e17055;"
        "background:transparent;border:none;}");
    stL->addWidget(mstH);
    m_mistakeTitle = new QLabel("", stepCard);
    m_mistakeTitle->setStyleSheet("QLabel{font-size:12px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    m_mistakeTitle->setWordWrap(true);
    stL->addWidget(m_mistakeTitle);
    m_mistakeDesc = new QLabel("", stepCard);
    m_mistakeDesc->setStyleSheet("QLabel{font-size:10px;color:#636e72;"
        "background:transparent;border:none;}");
    m_mistakeDesc->setWordWrap(true);
    stL->addWidget(m_mistakeDesc);
    m_mistakeTip = new QLabel("", stepCard);
    m_mistakeTip->setStyleSheet("QLabel{font-size:10px;color:#00b894;font-weight:600;"
        "background:transparent;border:none;}");
    m_mistakeTip->setWordWrap(true);
    stL->addWidget(m_mistakeTip);

    // ── Error buttons (inside step card, bottom) ──
    QFrame *errSep = new QFrame(stepCard);
    errSep->setFrameShape(QFrame::HLine);
    errSep->setStyleSheet("QFrame{background:#f0f2f5;max-height:1px;border:none;}");
    stL->addWidget(errSep);

    QHBoxLayout *erl = new QHBoxLayout();
    erl->setSpacing(8);

    auto mkEB = [&](const QString &txt, const QString &bg, const QString &fg, const QString &hv) -> QPushButton* {
        QPushButton *b = new QPushButton(txt, stepCard);
        b->setCursor(Qt::PointingHandCursor); b->setFixedHeight(34);
        b->setStyleSheet(QString("QPushButton{background:%1;color:%2;border:none;"
            "border-radius:8px;padding:0 16px;font-size:11px;font-weight:bold;}"
            "QPushButton:hover{background:%3;}").arg(bg, fg, hv));
        return b;
    };

    m_calageBtn = mkEB(QString::fromUtf8("\xf0\x9f\x9a\xab Stall"), "#fff5f0", "#e17055", "#fdebd0");
    connect(m_calageBtn, &QPushButton::clicked, this, &ParkingWidget::recordCalage);
    erl->addWidget(m_calageBtn);
    m_obstacleBtn = mkEB(QString::fromUtf8("\xf0\x9f\x92\xa5 Contact"), "#fef0f0", "#d63031", "#fab1a0");
    connect(m_obstacleBtn, &QPushButton::clicked, this, &ParkingWidget::recordObstacleContact);
    erl->addWidget(m_obstacleBtn);
    m_errorBtn = mkEB(QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f Error"), "#fef9e7", "#f39c12", "#fdebd0");
    connect(m_errorBtn, &QPushButton::clicked, this, [this](){ recordError("general"); });
    erl->addWidget(m_errorBtn);
    erl->addStretch();
    m_liveScoreLabel = new QLabel(QString::fromUtf8("Stalls: 0 \xc2\xb7 Contacts: 0 \xc2\xb7 Errors: 0"), stepCard);
    m_liveScoreLabel->setStyleSheet("QLabel{font-size:9px;color:#b2bec3;background:transparent;border:none;}");
    erl->addWidget(m_liveScoreLabel);
    stL->addLayout(erl);

    sL->addWidget(stepCard);

    // ────────────────────────────────────
    //  SECTION 2: Score (full width)
    // ────────────────────────────────────
    QFrame *scorePanel = new QFrame(this);
    scorePanel->setObjectName("scPnl2");
    scorePanel->setStyleSheet("QFrame#scPnl2{background:white;border-radius:14px;"
        "border:1px solid #e2e8f0;}");
    scorePanel->setGraphicsEffect(shadow(scorePanel));
    QHBoxLayout *scRow = new QHBoxLayout(scorePanel);
    scRow->setContentsMargins(24,16,24,16);
    scRow->setSpacing(20);

    // Score left
    QVBoxLayout *scLeft = new QVBoxLayout();
    scLeft->setSpacing(4);
    QLabel *scTitle = new QLabel(QString::fromUtf8("\xf0\x9f\x93\x8a Session Score"), scorePanel);
    scTitle->setStyleSheet("QLabel{font-size:12px;font-weight:bold;color:#636e72;"
        "background:transparent;border:none;}");
    scLeft->addWidget(scTitle);

    m_scoreLabel = new QLabel("--", scorePanel);
    m_scoreLabel->setStyleSheet("QLabel{font-size:42px;font-weight:bold;color:#00b894;"
        "background:transparent;border:none;}");
    scLeft->addWidget(m_scoreLabel);

    m_errorsLabel = new QLabel(QString::fromUtf8("Errors: 0 \xc2\xb7 Stalls: 0"), scorePanel);
    m_errorsLabel->setStyleSheet("QLabel{font-size:10px;color:#b2bec3;"
        "background:transparent;border:none;}");
    scLeft->addWidget(m_errorsLabel);
    scRow->addLayout(scLeft);

    scRow->addStretch();

    // ATTT rules right
    QFrame *vsep = new QFrame(scorePanel);
    vsep->setFrameShape(QFrame::VLine);
    vsep->setStyleSheet("QFrame{background:#e2e8f0;max-width:1px;border:none;}");
    scRow->addWidget(vsep);

    QVBoxLayout *scRight = new QVBoxLayout();
    scRight->setSpacing(3);
    QLabel *ruTitle = new QLabel(QString::fromUtf8("\xf0\x9f\x87\xb9\xf0\x9f\x87\xb3 ATTT Exam Rules"), scorePanel);
    ruTitle->setStyleSheet("QLabel{font-size:10px;font-weight:bold;color:#636e72;"
        "background:transparent;border:none;}");
    scRight->addWidget(ruTitle);

    for (const auto &r : QStringList{
        QString::fromUtf8("\xc2\xb7 3 min max per maneuver"),
        QString::fromUtf8("\xc2\xb7 Curb contact = eliminated"),
        QString::fromUtf8("\xc2\xb7 Obstacle hit = eliminated"),
        QString::fromUtf8("\xc2\xb7 2+ stalls = eliminated")}) {
        QLabel *rl = new QLabel(r, scorePanel);
        rl->setStyleSheet("QLabel{font-size:9px;color:#b2bec3;background:transparent;border:none;}");
        scRight->addWidget(rl);
    }
    scRow->addLayout(scRight);

    sL->addWidget(scorePanel);

    // ────────────────────────────────────
    //  SECTION 3: Car Diagram / Sensors (full width)
    // ────────────────────────────────────
    sL->addWidget(createCarDiagram());

    sL->addStretch();
    scroll->setWidget(scrollContent);
    pL->addWidget(scroll, 1);

    // ── Bottom Bar (fixed) ──
    pL->addWidget(createBottomBar());

    // ── Real Exam Instruction Card (big overlay in step area) ──
    m_examInstructionCard = new QFrame(scrollContent);
    m_examInstructionCard->setObjectName("examInstCard");
    m_examInstructionCard->setStyleSheet("QFrame#examInstCard{background:white;border-radius:16px;"
        "border:2px solid #00b894;}");
    m_examInstructionCard->setVisible(false);
    QVBoxLayout *eiL = new QVBoxLayout(m_examInstructionCard);
    eiL->setContentsMargins(24,20,24,20);
    eiL->setSpacing(8);
    eiL->setAlignment(Qt::AlignCenter);

    m_examBigIcon = new QLabel("", m_examInstructionCard);
    m_examBigIcon->setAlignment(Qt::AlignCenter);
    m_examBigIcon->setStyleSheet("QLabel{font-size:48px;background:transparent;border:none;}");
    eiL->addWidget(m_examBigIcon);

    m_examBigTitle = new QLabel("", m_examInstructionCard);
    m_examBigTitle->setAlignment(Qt::AlignCenter);
    m_examBigTitle->setWordWrap(true);
    m_examBigTitle->setStyleSheet("QLabel{font-size:22px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    eiL->addWidget(m_examBigTitle);

    m_examBigInstruction = new QLabel("", m_examInstructionCard);
    m_examBigInstruction->setAlignment(Qt::AlignCenter);
    m_examBigInstruction->setWordWrap(true);
    m_examBigInstruction->setStyleSheet("QLabel{font-size:14px;color:#636e72;"
        "background:transparent;border:none;}");
    eiL->addWidget(m_examBigInstruction);

    sL->addWidget(m_examInstructionCard);

    // ── Floating message ──
    m_floatingMsg = new QLabel("", page);
    m_floatingMsg->setAlignment(Qt::AlignCenter);
    m_floatingMsg->setFixedHeight(40);
    m_floatingMsg->setStyleSheet("QLabel{font-size:13px;font-weight:bold;color:white;"
        "background:#00b894;border-radius:12px;padding:8px 24px;border:none;}");
    m_floatingMsg->setVisible(false);
    m_floatingMsg->raise();

    // ═══════════════════════════════════════════════════════════
    //  FLOATING NOTES BUBBLE — WhatsApp / Snapchat style
    //  FAB button bottom-right + expandable note editor popup
    // ═══════════════════════════════════════════════════════════
    m_notesBubbleParent = page;
    m_notesBubbleOpen = false;

    // ── FAB (Floating Action Button) ──
    m_notesFAB = new QPushButton(QString::fromUtf8("\xf0\x9f\x93\x9d"), page);
    m_notesFAB->setFixedSize(56, 56);
    m_notesFAB->setCursor(Qt::PointingHandCursor);
    m_notesFAB->setStyleSheet(
        "QPushButton{"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #00b894, stop:1 #00a884);"
        "  color: white; font-size: 24px; border: none; border-radius: 28px;"
        "}"
        "QPushButton:hover{"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #00cca3, stop:1 #00b894);"
        "}"
        "QPushButton:pressed{ background: #009874; }"
    );
    m_notesFAB->setGraphicsEffect(shadow(m_notesFAB, 20, 40));
    connect(m_notesFAB, &QPushButton::clicked, this, &ParkingWidget::toggleNotesBubble);
    m_notesFAB->raise();

    // ── Stats FAB (floating bubble → Coach & Stats page) ──
    // 📊  U+1F4CA  →  UTF-8 F0 9F 93 8A
    m_statsFAB = new QPushButton(QString::fromUtf8("\xf0\x9f\x93\x8a"), page);
    m_statsFAB->setFixedSize(56, 56);
    m_statsFAB->setCursor(Qt::PointingHandCursor);
    m_statsFAB->setToolTip(QString::fromUtf8("Coach & Stats"));
    m_statsFAB->setStyleSheet(
        "QPushButton{"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #6c5ce7, stop:1 #5a4bd1);"
        "  color: white; font-size: 24px; border: none; border-radius: 28px;"
        "}"
        "QPushButton:hover{"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:1, stop:0 #7d6ff0, stop:1 #6c5ce7);"
        "}"
        "QPushButton:pressed{ background: #4a3bc0; }"
    );
    m_statsFAB->setGraphicsEffect(shadow(m_statsFAB, 20, 40));
    connect(m_statsFAB, &QPushButton::clicked, this, [this](){
        m_stack->setCurrentIndex(4);   // Coach & Stats page
    });
    m_statsFAB->raise();

    // ── Expandable Notes Bubble (popup editor) ──
    m_notesBubble = new QFrame(page);
    m_notesBubble->setObjectName("notesBbl");
    m_notesBubble->setFixedSize(310, 380);
    m_notesBubble->setStyleSheet(
        "QFrame#notesBbl{"
        "  background: white;"
        "  border-radius: 20px;"
        "  border-bottom-right-radius: 6px;"
        "  border: 1px solid #e2e8f0;"
        "}"
    );
    m_notesBubble->setGraphicsEffect(shadow(m_notesBubble, 28, 30));

    QVBoxLayout *nbL = new QVBoxLayout(m_notesBubble);
    nbL->setContentsMargins(0, 0, 0, 0);
    nbL->setSpacing(0);

    // ── Header bar (green, like WhatsApp) ──
    QFrame *nbHeader = new QFrame(m_notesBubble);
    nbHeader->setObjectName("nbHdr");
    nbHeader->setFixedHeight(52);
    nbHeader->setStyleSheet(
        "QFrame#nbHdr{"
        "  background: qlineargradient(x1:0,y1:0,x2:1,y2:0, stop:0 #00b894, stop:1 #00a884);"
        "  border-top-left-radius: 20px; border-top-right-radius: 20px;"
        "  border-bottom-left-radius: 0; border-bottom-right-radius: 0;"
        "  border: none;"
        "}"
    );
    QHBoxLayout *hdrL = new QHBoxLayout(nbHeader);
    hdrL->setContentsMargins(16, 8, 10, 8);
    hdrL->setSpacing(10);

    QLabel *nbIcon = new QLabel(QString::fromUtf8("\xf0\x9f\x93\x9d"), nbHeader);
    nbIcon->setStyleSheet("QLabel{font-size:20px;background:transparent;border:none;}");
    hdrL->addWidget(nbIcon);

    QVBoxLayout *hdrTxt = new QVBoxLayout();
    hdrTxt->setSpacing(0);
    m_notesBubbleTitle = new QLabel("Mes Notes", nbHeader);
    m_notesBubbleTitle->setStyleSheet("QLabel{font-size:14px;font-weight:bold;color:white;"
        "background:transparent;border:none;}");
    hdrTxt->addWidget(m_notesBubbleTitle);
    m_notesSaveStatus = new QLabel(QString::fromUtf8("\xe2\x9c\x93 Saved"), nbHeader);
    m_notesSaveStatus->setStyleSheet("QLabel{font-size:9px;color:rgba(255,255,255,0.7);"
        "background:transparent;border:none;}");
    hdrTxt->addWidget(m_notesSaveStatus);
    hdrL->addLayout(hdrTxt);
    hdrL->addStretch();

    // Close button
    QPushButton *nbClose = new QPushButton(QString::fromUtf8("\xe2\x9c\x95"), nbHeader);
    nbClose->setFixedSize(30, 30);
    nbClose->setCursor(Qt::PointingHandCursor);
    nbClose->setStyleSheet(
        "QPushButton{background:rgba(255,255,255,0.2);color:white;border:none;"
        "border-radius:15px;font-size:12px;font-weight:bold;}"
        "QPushButton:hover{background:rgba(255,255,255,0.35);}"
    );
    connect(nbClose, &QPushButton::clicked, this, &ParkingWidget::toggleNotesBubble);
    hdrL->addWidget(nbClose);
    nbL->addWidget(nbHeader);

    // ── Note text area (main content) ──
    m_notesTextEdit = new QTextEdit(m_notesBubble);
    m_notesTextEdit->setPlaceholderText(QString::fromUtf8(
        "Write your notes here...\n\n"
        "\xf0\x9f\x92\xa1 Tips, observations, mistakes to avoid..."));
    m_notesTextEdit->setStyleSheet(
        "QTextEdit{"
        "  background: #fafbfc; color: #2d3436; font-size: 13px;"
        "  border: none; padding: 14px 16px;"
        "  font-family: 'Segoe UI', sans-serif; line-height: 1.6;"
        "  selection-background-color: #00b894; selection-color: white;"
        "}"
        "QScrollBar:vertical{width:5px;background:transparent;}"
        "QScrollBar::handle:vertical{background:#d0d5db;border-radius:2px;min-height:20px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
    );
    nbL->addWidget(m_notesTextEdit, 1);

    // ── Bottom bar (timestamp + char count, like WhatsApp) ──
    QFrame *nbFooter = new QFrame(m_notesBubble);
    nbFooter->setObjectName("nbFoot");
    nbFooter->setFixedHeight(36);
    nbFooter->setStyleSheet(
        "QFrame#nbFoot{"
        "  background: #f5f6fa; border: none;"
        "  border-bottom-left-radius: 20px; border-bottom-right-radius: 6px;"
        "  border-top: 1px solid #eef0f2;"
        "}"
    );
    QHBoxLayout *footL = new QHBoxLayout(nbFooter);
    footL->setContentsMargins(14, 4, 14, 4);

    QLabel *footInfo = new QLabel(QString::fromUtf8("\xf0\x9f\x95\x90 Auto-save"), nbFooter);
    footInfo->setStyleSheet("QLabel{font-size:9px;color:#b2bec3;background:transparent;border:none;}");
    footL->addWidget(footInfo);
    footL->addStretch();
    QLabel *footCheck = new QLabel(QString::fromUtf8("\xe2\x9c\x93\xe2\x9c\x93"), nbFooter);
    footCheck->setStyleSheet("QLabel{font-size:10px;color:#00b894;background:transparent;border:none;}");
    footL->addWidget(footCheck);
    nbL->addWidget(nbFooter);

    m_notesBubble->setVisible(false);
    m_notesBubble->raise();

    // ── Auto-save timer ──
    m_notesAutoSave = new QTimer(this);
    m_notesAutoSave->setSingleShot(true);
    m_notesAutoSave->setInterval(1500);
    connect(m_notesAutoSave, &QTimer::timeout, this, &ParkingWidget::saveQuickNote);
    connect(m_notesTextEdit, &QTextEdit::textChanged, this, [this](){
        m_notesSaveStatus->setText(QString::fromUtf8("\xe2\x9c\x8f Writing..."));
        m_notesSaveStatus->setStyleSheet("QLabel{font-size:9px;color:rgba(255,255,255,0.9);"
            "background:transparent;border:none;}");
        m_notesAutoSave->start();
    });

    // Load existing note for current session
    loadQuickNote();

    return page;
}

// ═══════════════════════════════════════════════════════════════════
//  ERROR RECORDING PANEL
// ═══════════════════════════════════════════════════════════════════

QWidget* ParkingWidget::createErrorRecordingPanel()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("errPanel");
    card->setStyleSheet(cardSS("errPanel"));
    card->setGraphicsEffect(shadow(card,12,10));
    QVBoxLayout *l = new QVBoxLayout(card);
    l->setContentsMargins(16,12,16,12);
    l->setSpacing(8);

    QLabel *title = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x93\x9d  Record an error"), card);
    title->setStyleSheet("QLabel{font-size:12px;font-weight:bold;color:#636e72;background:transparent;border:none;}");
    l->addWidget(title);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    // Stall button
    m_calageBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x9a\xab Stall"), card);
    m_calageBtn->setCursor(Qt::PointingHandCursor);
    m_calageBtn->setFixedHeight(36);
    m_calageBtn->setStyleSheet("QPushButton{background:#fff8f0;color:#e17055;border:2px solid #fdebd0;"
        "border-radius:10px;padding:0 14px;font-size:11px;font-weight:bold;}"
        "QPushButton:hover{background:#fdebd0;}QPushButton:pressed{background:#e17055;color:white;}");
    connect(m_calageBtn, &QPushButton::clicked, this, &ParkingWidget::recordCalage);
    btnRow->addWidget(m_calageBtn);

    // Obstacle contact button
    m_obstacleBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x92\xa5 Contact"), card);
    m_obstacleBtn->setCursor(Qt::PointingHandCursor);
    m_obstacleBtn->setFixedHeight(36);
    m_obstacleBtn->setStyleSheet("QPushButton{background:#fef0f0;color:#d63031;border:2px solid #fab1a0;"
        "border-radius:10px;padding:0 14px;font-size:11px;font-weight:bold;}"
        "QPushButton:hover{background:#fab1a0;}QPushButton:pressed{background:#d63031;color:white;}");
    connect(m_obstacleBtn, &QPushButton::clicked, this, &ParkingWidget::recordObstacleContact);
    btnRow->addWidget(m_obstacleBtn);

    // Generic error button
    m_errorBtn = new QPushButton(QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f Error"), card);
    m_errorBtn->setCursor(Qt::PointingHandCursor);
    m_errorBtn->setFixedHeight(36);
    m_errorBtn->setStyleSheet("QPushButton{background:#fef3e2;color:#fdcb6e;border:2px solid #fdebd0;"
        "border-radius:10px;padding:0 14px;font-size:11px;font-weight:bold;}"
        "QPushButton:hover{background:#fdebd0;}QPushButton:pressed{background:#fdcb6e;color:white;}");
    connect(m_errorBtn, &QPushButton::clicked, this, [this](){ recordError("general"); });
    btnRow->addWidget(m_errorBtn);

    l->addLayout(btnRow);

    // Live counters
    m_liveScoreLabel = new QLabel(QString::fromUtf8(
        "Stalls: 0  \xc2\xb7  Contacts: 0  \xc2\xb7  Errors: 0"), card);
    m_liveScoreLabel->setStyleSheet("QLabel{font-size:10px;color:#b2bec3;background:transparent;border:none;}");
    l->addWidget(m_liveScoreLabel);

    return card;
}

QWidget* ParkingWidget::createStepCard()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("stepC");
    card->setStyleSheet(cardSS("stepC"));
    card->setGraphicsEffect(shadow(card));
    QVBoxLayout *l = new QVBoxLayout(card);
    l->setContentsMargins(24,20,24,20);
    l->setSpacing(14);

    // Header row with title and step counter
    QHBoxLayout *hdrRow = new QHBoxLayout();
    QLabel *hdr = new QLabel(QString::fromUtf8("Step-by-Step Guide"), card);
    hdr->setStyleSheet("QLabel{font-size:15px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
    hdrRow->addWidget(hdr);
    hdrRow->addStretch();
    l->addLayout(hdrRow);

    // Step progress dots — horizontal colored bar
    m_dotsLayout = new QHBoxLayout();
    m_dotsLayout->setSpacing(4);
    m_dotsLayout->setAlignment(Qt::AlignLeft);
    l->addLayout(m_dotsLayout);

    // Step content: number circle + instruction
    QHBoxLayout *mainRow = new QHBoxLayout();
    mainRow->setSpacing(16);

    // Step number — clean teal circle
    m_stepNum = new QLabel("1", card);
    m_stepNum->setFixedSize(52,52);
    m_stepNum->setAlignment(Qt::AlignCenter);
    m_stepNum->setStyleSheet("QLabel{background:#00b894;color:white;font-size:22px;font-weight:bold;"
        "border-radius:26px;border:none;}");
    mainRow->addWidget(m_stepNum, 0, Qt::AlignTop);

    QVBoxLayout *titleCol = new QVBoxLayout();
    titleCol->setSpacing(6);

    m_stepTitle = new QLabel(QString::fromUtf8("Select a maneuver"), card);
    m_stepTitle->setStyleSheet("QLabel{font-size:16px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
    m_stepTitle->setWordWrap(true);
    titleCol->addWidget(m_stepTitle);

    m_stepInstruction = new QLabel(QString::fromUtf8("Choose from the home page to start."), card);
    m_stepInstruction->setStyleSheet("QLabel{font-size:12px;color:#636e72;background:transparent;border:none;line-height:1.5;}");
    m_stepInstruction->setWordWrap(true);
    titleCol->addWidget(m_stepInstruction);

    mainRow->addLayout(titleCol, 1);
    l->addLayout(mainRow);

    // Separator
    QFrame *sep = new QFrame(card);
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("QFrame{background:#eaeaea;max-height:1px;border:none;}");
    l->addWidget(sep);

    // Tip box — clean light background
    QWidget *tipBox = new QWidget(card);
    tipBox->setObjectName("tipB");
    tipBox->setStyleSheet("QWidget#tipB{background:#f5f5f5;border-radius:10px;border:none;}");
    QVBoxLayout *tbl = new QVBoxLayout(tipBox);
    tbl->setContentsMargins(14,10,14,10);
    tbl->setSpacing(4);
    QLabel *tipH = new QLabel(QString::fromUtf8("\xf0\x9f\x92\xa1 Tip"), tipBox);
    tipH->setStyleSheet("QLabel{font-size:11px;font-weight:bold;color:#00b894;background:transparent;border:none;}");
    tbl->addWidget(tipH);
    m_stepTip = new QLabel("", tipBox);
    m_stepTip->setWordWrap(true);
    m_stepTip->setStyleSheet("QLabel{font-size:11px;color:#636e72;background:transparent;border:none;line-height:1.4;}");
    tbl->addWidget(m_stepTip);
    l->addWidget(tipBox);

    // Sensor info — blue accent
    QWidget *sensorBox = new QWidget(card);
    sensorBox->setObjectName("senB");
    sensorBox->setStyleSheet("QWidget#senB{background:#e3f2fd;border-radius:10px;border:none;}");
    QHBoxLayout *sbl = new QHBoxLayout(sensorBox);
    sbl->setContentsMargins(14,8,14,8);
    m_stepSensor = new QLabel("", sensorBox);
    m_stepSensor->setWordWrap(true);
    m_stepSensor->setStyleSheet("QLabel{font-size:11px;color:#0984e3;background:transparent;border:none;}");
    sbl->addWidget(m_stepSensor);
    l->addWidget(sensorBox);

    // Guidance message — dynamic feedback
    m_guidanceMsg = new QLabel("", card);
    m_guidanceMsg->setWordWrap(true);
    m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#00b894;font-weight:bold;"
        "background:#e8f8f5;border-radius:10px;padding:10px 14px;border:none;}");
    m_guidanceMsg->setVisible(false);
    l->addWidget(m_guidanceMsg);

    return card;
}

QWidget* ParkingWidget::createCarDiagram()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("carC");
    card->setStyleSheet(cardSS("carC"));
    card->setGraphicsEffect(shadow(card));
    QVBoxLayout *l = new QVBoxLayout(card);
    l->setContentsMargins(18,14,18,14);
    l->setSpacing(10);

    // Header row
    QHBoxLayout *hdr = new QHBoxLayout();
    QLabel *t = new QLabel(QString::fromUtf8("\xf0\x9f\x93\xa1  Sensors"), card);
    t->setStyleSheet("QLabel{font-size:13px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
    hdr->addWidget(t);
    hdr->addStretch();
    m_connStatus = new QLabel(QString::fromUtf8("\xf0\x9f\x94\xb4 Disconnected"), card);
    m_connStatus->setStyleSheet("QLabel{font-size:9px;color:#e17055;background:#fef3e2;"
        "border-radius:8px;padding:3px 8px;border:none;}");
    hdr->addWidget(m_connStatus);
    l->addLayout(hdr);

    // ── TOP-DOWN CAR VISUAL ──
    // Grid layout: 3 cols x 5 rows
    //   Row 0:        [front sensor]
    //   Row 1: [left] [  car body  ] [right]
    //   Row 2: [rearL][            ] [rearR]
    QGridLayout *carGrid = new QGridLayout();
    carGrid->setSpacing(4);
    carGrid->setAlignment(Qt::AlignCenter);

    auto makeSensor = [&](const QString &label, const QString &defaultColor) -> QPair<QLabel*,QLabel*> {
        QLabel *zone = new QLabel("", card);
        zone->setFixedSize(60, 28);
        zone->setAlignment(Qt::AlignCenter);
        zone->setStyleSheet(QString("QLabel{background:%1;border-radius:6px;border:none;}").arg(defaultColor));

        QLabel *dist = new QLabel("--", card);
        dist->setAlignment(Qt::AlignCenter);
        dist->setStyleSheet("QLabel{font-size:9px;color:#636e72;background:transparent;border:none;"
            "font-family:'Courier New',monospace;}");
        Q_UNUSED(label);
        return {zone, dist};
    };

    // Front sensor (top center)
    auto pf = makeSensor("Avant", "#e8f8f5");
    m_zFront = pf.first; m_dFront = pf.second;
    QVBoxLayout *frontCol = new QVBoxLayout();
    frontCol->setAlignment(Qt::AlignCenter);
    frontCol->setSpacing(2);
    QLabel *fLbl = new QLabel("AV", card);
    fLbl->setAlignment(Qt::AlignCenter);
    fLbl->setStyleSheet("QLabel{font-size:8px;font-weight:bold;color:#b2bec3;background:transparent;border:none;}");
    frontCol->addWidget(fLbl);
    frontCol->addWidget(m_zFront);
    frontCol->addWidget(m_dFront);
    carGrid->addLayout(frontCol, 0, 1, Qt::AlignCenter);

    // Car body (center)
    QLabel *body = new QLabel(QString::fromUtf8("\xf0\x9f\x9a\x97"), card);
    body->setFixedSize(80, 100);
    body->setAlignment(Qt::AlignCenter);
    body->setStyleSheet("QLabel{background:#e8f8f5;border-radius:12px;font-size:36px;"
        "border:2px solid #d1f2eb;}");
    carGrid->addWidget(body, 1, 1, 2, 1, Qt::AlignCenter);

    // Left sensor
    auto pl = makeSensor("Gauche", "#e8f8f5");
    m_zLeft = pl.first; m_dLeft = pl.second;
    QVBoxLayout *leftC = new QVBoxLayout();
    leftC->setAlignment(Qt::AlignCenter);
    leftC->setSpacing(2);
    QLabel *lLbl = new QLabel("G", card);
    lLbl->setAlignment(Qt::AlignCenter);
    lLbl->setStyleSheet("QLabel{font-size:8px;font-weight:bold;color:#b2bec3;background:transparent;border:none;}");
    leftC->addWidget(lLbl);
    leftC->addWidget(m_zLeft);
    leftC->addWidget(m_dLeft);
    carGrid->addLayout(leftC, 1, 0, Qt::AlignCenter);

    // Right sensor
    auto pr = makeSensor("Droite", "#e8f8f5");
    m_zRight = pr.first; m_dRight = pr.second;
    QVBoxLayout *rightC = new QVBoxLayout();
    rightC->setAlignment(Qt::AlignCenter);
    rightC->setSpacing(2);
    QLabel *rLbl = new QLabel("D", card);
    rLbl->setAlignment(Qt::AlignCenter);
    rLbl->setStyleSheet("QLabel{font-size:8px;font-weight:bold;color:#b2bec3;background:transparent;border:none;}");
    rightC->addWidget(rLbl);
    rightC->addWidget(m_zRight);
    rightC->addWidget(m_dRight);
    carGrid->addLayout(rightC, 1, 2, Qt::AlignCenter);

    // Rear Left
    auto prl = makeSensor("Arr.G", "#e8f8f5");
    m_zRearL = prl.first; m_dRearL = prl.second;
    QVBoxLayout *rlC = new QVBoxLayout();
    rlC->setAlignment(Qt::AlignCenter);
    rlC->setSpacing(2);
    rlC->addWidget(m_zRearL);
    rlC->addWidget(m_dRearL);
    QLabel *rlLbl = new QLabel("AG", card);
    rlLbl->setAlignment(Qt::AlignCenter);
    rlLbl->setStyleSheet("QLabel{font-size:8px;font-weight:bold;color:#b2bec3;background:transparent;border:none;}");
    rlC->addWidget(rlLbl);
    carGrid->addLayout(rlC, 3, 0, Qt::AlignCenter);

    // Rear Right
    auto prr = makeSensor("Arr.D", "#e8f8f5");
    m_zRearR = prr.first; m_dRearR = prr.second;
    QVBoxLayout *rrC = new QVBoxLayout();
    rrC->setAlignment(Qt::AlignCenter);
    rrC->setSpacing(2);
    rrC->addWidget(m_zRearR);
    rrC->addWidget(m_dRearR);
    QLabel *rrLbl = new QLabel("AD", card);
    rrLbl->setAlignment(Qt::AlignCenter);
    rrLbl->setStyleSheet("QLabel{font-size:8px;font-weight:bold;color:#b2bec3;background:transparent;border:none;}");
    rrC->addWidget(rrLbl);
    carGrid->addLayout(rrC, 3, 2, Qt::AlignCenter);

    l->addLayout(carGrid);

    // ── Simulation trigger buttons (shown only when Sim mode is ON) ──
    m_simButtonPanel = new QFrame(card);
    m_simButtonPanel->setVisible(false);
    m_simButtonPanel->setStyleSheet(
        "QFrame{background:#eef4ff;border-radius:10px;"
        "border:1.5px dashed #0984e3;padding:2px;}");
    QVBoxLayout *simPL = new QVBoxLayout(m_simButtonPanel);
    simPL->setContentsMargins(8, 5, 8, 5);
    simPL->setSpacing(4);

    QLabel *simHint = new QLabel(
        QString::fromUtf8("\xf0\x9f\x8e\xae  Sim: press a sensor to trigger detection"),
        m_simButtonPanel);
    simHint->setStyleSheet(
        "QLabel{font-size:9px;color:#0984e3;background:transparent;"
        "border:none;font-weight:bold;}");
    simHint->setAlignment(Qt::AlignCenter);
    simPL->addWidget(simHint);

    QHBoxLayout *simBtnsL = new QHBoxLayout();
    simBtnsL->setSpacing(5);

    // Each button fires the primary detection event for that sensor zone
    struct SimBtn { QString label; QString event; QString color; QString tip; };
    QList<SimBtn> simDefs = {
        {"AV",  "FRONT_OBSTACLE",    "#6c5ce7", "Front obstacle detected"},
        {"G",   "LEFT_OBSTACLE",     "#e17055", "Left sensor: obstacle"},
        {"D",   "RIGHT_OBSTACLE",    "#0984e3", "Right sensor: obstacle"},
        {"AG",  "BOTH_REAR_OBSTACLE","#00b894", "Both rear sensors: obstacle"},
        {"AD",  "REAR_RIGHT_SENSOR", "#fdcb6e", "Rear-right sensor triggered"},
    };

    for (const SimBtn &def : simDefs) {
        QPushButton *btn = new QPushButton(def.label, m_simButtonPanel);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(28);
        btn->setToolTip(def.tip + "\n→ fires event: " + def.event);
        btn->setStyleSheet(QString(
            "QPushButton{background:%1;color:white;border:none;border-radius:7px;"
            "font-size:10px;font-weight:bold;}"
            "QPushButton:hover{background:#2d3436;}"
            "QPushButton:pressed{background:#636e72;}").arg(def.color));
        const QString ev = def.event;
        connect(btn, &QPushButton::clicked, this, [this, ev](){
            if (m_simulationMode) m_sensorMgr->simulateEvent(ev);
        });
        simBtnsL->addWidget(btn);
    }
    simPL->addLayout(simBtnsL);

    // Secondary row: the remaining sensor-specific events needed for maneuver steps
    QHBoxLayout *simBtns2L = new QHBoxLayout();
    simBtns2L->setSpacing(5);
    QList<SimBtn> simDefs2 = {
        {"G-S1",  "LEFT_SENSOR1",    "#a29bfe", "Left sensor 1 (45° angle)"},
        {"D-S2",  "RIGHT_SENSOR2",   "#74b9ff", "Right sensor 2 (alignment)"},
        {"AG+AD", "BOTH_REAR_SENSOR","#55efc4", "Both rear sensors: final position"},
    };
    for (const SimBtn &def : simDefs2) {
        QPushButton *btn = new QPushButton(def.label, m_simButtonPanel);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setFixedHeight(24);
        btn->setToolTip(def.tip + "\n→ fires event: " + def.event);
        btn->setStyleSheet(QString(
            "QPushButton{background:%1;color:white;border:none;border-radius:6px;"
            "font-size:9px;font-weight:bold;}"
            "QPushButton:hover{background:#2d3436;}"
            "QPushButton:pressed{background:#636e72;}").arg(def.color));
        const QString ev = def.event;
        connect(btn, &QPushButton::clicked, this, [this, ev](){
            if (m_simulationMode) m_sensorMgr->simulateEvent(ev);
        });
        simBtns2L->addWidget(btn);
    }
    simPL->addLayout(simBtns2L);

    l->addWidget(m_simButtonPanel);

    // Connect button
    QPushButton *conn = new QPushButton(QString::fromUtf8("\xf0\x9f\x94\x8c  Connect Model"), card);
    conn->setCursor(Qt::PointingHandCursor);
    conn->setFixedHeight(32);
    conn->setStyleSheet("QPushButton{background:#0984e3;color:white;border:none;border-radius:10px;"
        "padding:0 14px;font-size:11px;font-weight:bold;}"
        "QPushButton:hover{background:#0870c0;}");
    connect(conn, &QPushButton::clicked, this, &ParkingWidget::connectMaquette);
    l->addWidget(conn);

    return card;
}

QWidget* ParkingWidget::createMiniStatusCard()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("msC");
    card->setStyleSheet(cardSS("msC"));
    card->setGraphicsEffect(shadow(card));
    QVBoxLayout *l = new QVBoxLayout(card);
    l->setContentsMargins(18,14,18,14);
    l->setSpacing(8);
    QLabel *t = new QLabel(QString::fromUtf8("\xf0\x9f\x93\x8a  Session"), card);
    t->setStyleSheet("QLabel{font-size:13px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
    l->addWidget(t);
    m_scoreLabel = new QLabel("Score: --", card);
    m_scoreLabel->setStyleSheet("QLabel{font-size:16px;font-weight:bold;color:#00b894;background:transparent;border:none;}");
    l->addWidget(m_scoreLabel);
    m_errorsLabel = new QLabel(QString::fromUtf8("Errors: 0  \xc2\xb7  Stalls: 0"), card);
    m_errorsLabel->setStyleSheet("QLabel{font-size:11px;color:#636e72;background:transparent;border:none;}");
    l->addWidget(m_errorsLabel);

    // Exam criteria reminder
    QWidget *examBox = new QWidget(card);
    examBox->setObjectName("exB");
    examBox->setStyleSheet("QWidget#exB{background:#fff8f0;border-radius:10px;border:none;}");
    QVBoxLayout *el = new QVBoxLayout(examBox);
    el->setContentsMargins(10,8,10,8);
    el->setSpacing(3);
    QLabel *et = new QLabel(QString::fromUtf8("\xf0\x9f\x87\xb9\xf0\x9f\x87\xb3 Exam ATTT \xe2\x80\x94 Parc Manoeuvres"), examBox);
    et->setStyleSheet("QLabel{font-size:10px;font-weight:bold;color:#e17055;background:transparent;border:none;}");
    el->addWidget(et);
    for(const auto &cr : QStringList{
        QString::fromUtf8("\xf0\x9f\x93\x8d Closed course, solo driving"),
        QString::fromUtf8("\xe2\x8f\xb1 3 min max per maneuver"),
        QString::fromUtf8("\xe2\x9d\x8c Hitting curb = eliminated"),
        QString::fromUtf8("\xe2\x9d\x8c Obstacle collision = eliminated"),
        QString::fromUtf8("\xe2\x9d\x8c No visual check = eliminated"),
        QString::fromUtf8("\xe2\x9d\x8c Repeated stalls = eliminated"),
        QString::fromUtf8("\xf0\x9f\x94\x84 2 tentatives max (sinon repasser circulation)"),
        QString::fromUtf8("\xe2\x9c\x85 Light curb touch = tolerated"),
        QString::fromUtf8("\xf0\x9f\x93\x8a 56% pass rate first try in Tunisia")}){
        QLabel *fl = new QLabel(cr, examBox);
        fl->setStyleSheet("QLabel{font-size:9px;color:#636e72;background:transparent;border:none;}");
        el->addWidget(fl);
    }
    l->addWidget(examBox);
    return card;
}

QWidget* ParkingWidget::createMistakeCard()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("mstC");
    card->setStyleSheet(cardSS("mstC"));
    card->setGraphicsEffect(shadow(card));
    QVBoxLayout *l = new QVBoxLayout(card);
    l->setContentsMargins(18,14,18,14);
    l->setSpacing(8);

    QLabel *t = new QLabel(QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f  Common Mistake"), card);
    t->setStyleSheet("QLabel{font-size:13px;font-weight:bold;color:#e17055;background:transparent;border:none;}");
    l->addWidget(t);

    QWidget *mb = new QWidget(card);
    mb->setObjectName("mstB");
    mb->setStyleSheet("QWidget#mstB{background:#fff8f0;border-radius:10px;border:1px solid #fdebd0;}");
    QVBoxLayout *ml = new QVBoxLayout(mb);
    ml->setContentsMargins(12,10,12,10);
    ml->setSpacing(5);
    m_mistakeTitle = new QLabel("", mb);
    m_mistakeTitle->setStyleSheet("QLabel{font-size:12px;font-weight:bold;color:#e17055;background:transparent;border:none;}");
    m_mistakeTitle->setWordWrap(true);
    ml->addWidget(m_mistakeTitle);
    m_mistakeDesc = new QLabel("", mb);
    m_mistakeDesc->setWordWrap(true);
    m_mistakeDesc->setStyleSheet("QLabel{font-size:11px;color:#636e72;background:transparent;border:none;}");
    ml->addWidget(m_mistakeDesc);
    m_mistakeTip = new QLabel("", mb);
    m_mistakeTip->setWordWrap(true);
    m_mistakeTip->setStyleSheet("QLabel{font-size:11px;color:#00b894;font-weight:bold;"
        "background:#e8f8f5;border-radius:8px;padding:6px 10px;border:none;}");
    m_mistakeTip->setVisible(false);
    ml->addWidget(m_mistakeTip);
    l->addWidget(mb);
    return card;
}

QWidget* ParkingWidget::createBottomBar()
{
    QWidget *bar = new QWidget(this);
    bar->setObjectName("btmBar");
    bar->setFixedHeight(60);
    bar->setStyleSheet("QWidget#btmBar{background:white;border-top:2px solid #e2e8f0;}");
    QHBoxLayout *l = new QHBoxLayout(bar);
    l->setContentsMargins(28,0,28,0);
    l->setSpacing(14);

    m_prevBtn = new QPushButton(QString::fromUtf8("←  Previous"), bar);
    m_prevBtn->setCursor(Qt::PointingHandCursor);
    m_prevBtn->setFixedHeight(42);
    m_prevBtn->setStyleSheet("QPushButton{background:#f0f2f5;color:#636e72;border:1px solid #e2e8f0;"
        "border-radius:12px;padding:0 22px;font-size:12px;font-weight:bold;}"
        "QPushButton:hover{background:#e2e8f0;color:#2d3436;}");
    connect(m_prevBtn, &QPushButton::clicked, this, &ParkingWidget::prevStep);
    m_prevBtn->setVisible(false);
    l->addWidget(m_prevBtn);

    l->addStretch();

    m_endBtn = new QPushButton(QString::fromUtf8("✖  Finish"), bar);
    m_endBtn->setCursor(Qt::PointingHandCursor);
    m_endBtn->setFixedHeight(42);
    m_endBtn->setStyleSheet("QPushButton{background:#e17055;color:white;border:none;border-radius:12px;"
        "padding:0 24px;font-size:12px;font-weight:bold;letter-spacing:0.5px;}"
        "QPushButton:hover{background:#d63031;}");
    connect(m_endBtn, &QPushButton::clicked, this, &ParkingWidget::endSession);
    m_endBtn->setVisible(false);
    l->addWidget(m_endBtn);

    m_nextBtn = new QPushButton(QString::fromUtf8("Next  →"), bar);
    m_nextBtn->setCursor(Qt::PointingHandCursor);
    m_nextBtn->setFixedHeight(42);
    m_nextBtn->setStyleSheet(
        "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #00b894,stop:1 #55efc4);color:white;border:none;border-radius:12px;"
        "padding:0 28px;font-size:13px;font-weight:bold;letter-spacing:0.5px;}"
        "QPushButton:hover{background:#009874;}");
    connect(m_nextBtn, &QPushButton::clicked, this, &ParkingWidget::advanceStep);
    m_nextBtn->setVisible(false);
    l->addWidget(m_nextBtn);

    return bar;
}

// ═══════════════════════════════════════════════════════════════════
//  FLOW LOGIC
// ═══════════════════════════════════════════════════════════════════

void ParkingWidget::openManeuver(int idx)
{
    m_currentManeuver = idx;
    m_currentStep = 0;
    m_totalSteps = m_allManeuvers[idx].size();
    
    // In Full Exam mode, skip the very first Step (Safety checks) for subsequent maneuvers
    // because the user has already performed them at the beginning of the exam.
    if (m_fullExamMode && m_fullExamPhase > 0 && m_totalSteps > 1) {
        m_currentStep = 1; 
    }
    
    m_sessionTitle->setText(m_maneuverIcons[idx] + "  " + m_maneuverNames[idx]);

    // Load DB guidance messages for this maneuver type
    loadDbSensorMessages(m_maneuverDbKeys.value(idx, "creneau"));
    if (m_dbMsgLabel) m_dbMsgLabel->setVisible(false);

    updateStepUI();
    updateDots();
    updateCommonMistakes();

    // Hide session controls until checklist done
    m_prevBtn->setVisible(false);
    m_nextBtn->setVisible(false);
    m_endBtn->setVisible(false);
    m_guidanceMsg->setVisible(false);
    m_timerLabel->setText("00:00");
    m_scoreLabel->setText("Score: --");
    m_errorsLabel->setText(QString::fromUtf8("Errors: 0  \xc2\xb7  Stalls: 0"));
    m_examTimerLabel->setVisible(false);
    if(m_liveScoreLabel)
        m_liveScoreLabel->setText(QString::fromUtf8(
            "Stalls: 0  \xc2\xb7  Contacts: 0  \xc2\xb7  Errors: 0"));

    m_stack->setCurrentIndex(2);

    // Show checklist after a brief moment
    QTimer::singleShot(300, this, &ParkingWidget::showChecklistThenStart);
}

void ParkingWidget::showChecklistThenStart()
{
    // In full exam mode, only show the safety checklist at the very start (fwd = phase 0).
    // For subsequent maneuvers (creneau, demi-tour, marche_arriere) skip directly to session.
    if (m_fullExamMode && m_fullExamPhase > 0) {
        startSession();
        return;
    }

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("Safety Checklist"));
    dlg.setFixedWidth(380);
    dlg.setStyleSheet("QDialog{background:white;border-radius:16px;}"
        "QLabel{background:transparent;border:none;}");

    QVBoxLayout *l = new QVBoxLayout(&dlg);
    l->setContentsMargins(24,20,24,20);
    l->setSpacing(12);

    QLabel *title = new QLabel(QString::fromUtf8("\xe2\x9c\x85  Pre-maneuver checks"), &dlg);
    title->setStyleSheet("QLabel{font-size:16px;font-weight:bold;color:#2d3436;}");
    l->addWidget(title);

    QLabel *sub = new QLabel(QString::fromUtf8("\xf0\x9f\x87\xb9\xf0\x9f\x87\xb3 Just like at the ATTT course : check all before starting.\nForgotten check = eliminatory fault at the exam!"), &dlg);
    sub->setStyleSheet("QLabel{font-size:11px;color:#636e72;}");
    l->addWidget(sub);

    QStringList items = {
        QString::fromUtf8("Mirrors adjusted (interior + exterior)"),
        QString::fromUtf8("Seatbelt fastened"),
        QString::fromUtf8("Vehicle in neutral, handbrake engaged"),
        QString::fromUtf8("Blind spots checked (turn your head)"),
        QString::fromUtf8("Indicator on (maneuver direction)"),
        QString::fromUtf8("Doors closed, visibility OK")
    };

    QList<QCheckBox*> boxes;
    for(const auto &item : items){
        QCheckBox *cb = new QCheckBox(item, &dlg);
        cb->setStyleSheet("QCheckBox{font-size:13px;color:#2d3436;spacing:10px;}"
            "QCheckBox::indicator{width:20px;height:20px;border-radius:5px;border:2px solid #b2bec3;}"
            "QCheckBox::indicator:checked{background:#00b894;border-color:#00b894;}");
        boxes.append(cb);
        l->addWidget(cb);
    }

    // Buttons row: Select All + Start
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(10);

    QPushButton *selectAllBtn = new QPushButton(QString::fromUtf8("\xe2\x9c\x85  Select All"), &dlg);
    selectAllBtn->setFixedHeight(44);
    selectAllBtn->setCursor(Qt::PointingHandCursor);
    selectAllBtn->setStyleSheet("QPushButton{background:#f0f2f5;color:#636e72;border:none;"
        "border-radius:14px;font-size:12px;font-weight:bold;}"
        "QPushButton:hover{background:#e2e8f0;}");

    QPushButton *startBtn = new QPushButton(QString::fromUtf8("\xf0\x9f\x9a\x80  Start the maneuver"), &dlg);
    startBtn->setFixedHeight(44);
    startBtn->setCursor(Qt::PointingHandCursor);
    startBtn->setEnabled(false);
    startBtn->setStyleSheet("QPushButton{background:#dfe6e9;color:#b2bec3;border:none;border-radius:14px;"
        "font-size:14px;font-weight:bold;}");

    auto updateBtn = [startBtn, selectAllBtn, &boxes](){
        bool all = true;
        for(auto *b : boxes) if(!b->isChecked()){ all = false; break; }
        startBtn->setEnabled(all);
        startBtn->setStyleSheet(all ?
            "QPushButton{background:#00b894;"
            "color:white;border:none;border-radius:14px;font-size:14px;font-weight:bold;}"
            "QPushButton:hover{background:#00a382;}" :
            "QPushButton{background:#dfe6e9;color:#b2bec3;border:none;border-radius:14px;"
            "font-size:14px;font-weight:bold;}");
        selectAllBtn->setVisible(!all);
    };

    connect(selectAllBtn, &QPushButton::clicked, &dlg, [&boxes, &updateBtn](){
        for(auto *b : boxes) b->setChecked(true);
        updateBtn();
    });

    for(auto *cb : boxes)
        connect(cb, &QCheckBox::toggled, &dlg, updateBtn);

    connect(startBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    btnRow->addWidget(selectAllBtn);
    btnRow->addWidget(startBtn, 1);
    l->addLayout(btnRow);

    if(dlg.exec() == QDialog::Accepted){
        startSession();
    }
}

void ParkingWidget::startSession()
{
    m_sessionActive = true;
    m_sessionSeconds = 0;
    m_currentStep = 0;
    m_nbErrors = 0;
    m_nbCalages = 0;
    m_contactObstacle = false;
    m_simEventIndex = 0;
    m_sessionTimer->start(1000);
    m_timerLabel->setText("00:00");

    // DB — store using DB key (e.g. "creneau"), not the display name ("Parallel")
    QString manType = m_maneuverDbKeys[m_currentManeuver];
    m_currentSessionId = ParkingDBManager::instance().startSession(
        m_currentEleveId, m_currentVehiculeId, manType, m_examMode);

    updateStepUI();
    updateDots();
    updateCommonMistakes();
    updateLiveScore();

    // Show controls
    m_nextBtn->setVisible(!m_freeTrainingMode);
    m_endBtn->setVisible(true);
    m_prevBtn->setVisible(false);

    if(m_examMode){
        m_examTimerLabel->setVisible(true);
        m_examTimerLabel->setText("03:00");
        startRealExam();
    }

    QString modeStr = m_freeTrainingMode ?
        QString::fromUtf8("\xf0\x9f\x8e\xaf  Free mode \xe2\x80\x94 use the sensors !") :
        QString::fromUtf8("\xf0\x9f\x9f\xa2  Let's go! Follow the steps.");
    m_guidanceMsg->setText(modeStr);
    m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#00b894;font-weight:bold;"
        "background:#e8f8f5;border-radius:10px;padding:10px 14px;border:none;}");
    m_guidanceMsg->setVisible(true);

    showFloatingMessage(QString::fromUtf8("\xf0\x9f\x9a\x80 Session started!"), "#00b894");

    // Reposition notes FAB
    repositionNotesBubble();

    // Voice: announce session start + first step
    QString manName = m_maneuverNames[m_currentManeuver];
    speak(QString::fromUtf8("Session started. %1. %2")
        .arg(manName)
        .arg(m_examMode ? "Exam mode, you have 3 minutes." :
             m_freeTrainingMode ? "Free mode." : "Training mode."));
    // Speak first step after a short delay
    QTimer::singleShot(2500, this, [this]() { speakStep(0); });
}

void ParkingWidget::advanceStep()
{
    if(!m_sessionActive) return;
    m_currentStep++;
    if(m_currentStep >= m_totalSteps){
        endSession();
        return;
    }
    updateStepUI();
    updateDots();
    updateCommonMistakes();
    m_prevBtn->setVisible(m_currentStep > 0);
    m_guidanceMsg->setText(QString::fromUtf8(
        "\xe2\x9c\x85 Step validated!"));
    m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#00b894;font-weight:bold;"
        "background:#e8f8f5;border-radius:10px;padding:10px 14px;border:none;}");
    m_guidanceMsg->setVisible(true);

    showFloatingMessage(QString::fromUtf8("Step %1/%2")
        .arg(m_currentStep+1).arg(m_totalSteps), "#00b894");

    // Voice: announce new step
    speakStep(m_currentStep);
}

void ParkingWidget::prevStep()
{
    if(!m_sessionActive || m_currentStep <= 0) return;
    m_currentStep--;
    updateStepUI();
    updateDots();
    updateCommonMistakes();
    m_prevBtn->setVisible(m_currentStep > 0);
}

void ParkingWidget::endSession()
{
    m_sessionActive = false;
    m_sessionTimer->stop();
    // Save notes on session end
    if(m_notesAutoSave && m_notesAutoSave->isActive()) { m_notesAutoSave->stop(); saveQuickNote(); }
    bool success = (m_currentStep >= m_totalSteps) && !m_contactObstacle && m_nbCalages < 2;
    if(m_examMode && m_sessionSeconds > 180) success = false;
    int score = calculateScore(success);

    ParkingDBManager::instance().endSession(m_currentSessionId, success,
        m_sessionSeconds, m_currentStep, m_nbErrors, m_nbCalages, m_contactObstacle);

    // Save exam result if in exam mode
    if (m_examMode) {
        double scoreSur20 = score * 20.0 / 100.0;
        QString fauteElim;
        if (m_contactObstacle) fauteElim = "Contact obstacle";
        else if (m_nbCalages >= 2) fauteElim = QString::fromUtf8("Excessive stalls (%1)").arg(m_nbCalages);
        else if (m_sessionSeconds > 180) fauteElim = QString::fromUtf8("Time exceeded");

        QString manType = m_maneuverDbKeys[m_currentManeuver];
        ParkingDBManager::instance().saveResultatExamen(
            m_currentEleveId, manType, m_sessionSeconds, success,
            scoreSur20, m_nbErrors, fauteElim, "");
    }

    // Persist stats to Oracle (MERGE INTO PARKING_STUDENT_STATS)
    ParkingDBManager::instance().updateStatistiquesEleve(m_currentEleveId);

    // ── Reload ALL counters from Oracle — Oracle is the single source of truth ──
    // This guarantees that each student's dashboard reflects exactly what is in the DB,
    // regardless of which account is logged in.
    loadStatsFromDB();

    // XP / level are gamification-only (not persisted in DB), keep in-memory
    if(success){
        m_totalXP += m_examMode ? 50 : 25;
    } else {
        m_totalXP += 10;
    }
    m_currentLevel = 1 + m_totalXP / 100;

    // Voice announcement
    speak(success ?
        QString::fromUtf8("Session ended") :
        QString::fromUtf8("Session ended. Score %1 out of 100. %2 errors, %3 stalls.")
            .arg(score).arg(m_nbErrors).arg(m_nbCalages));

    m_scoreLabel->setText(QString("Score: %1/100").arg(score));
    QString atttGrade = score >= 80 ? "A" : score >= 50 ? "B" : "C";
    QString scCol = score >= 80 ? "#00b894" : score >= 50 ? "#fdcb6e" : "#e17055";
    m_scoreLabel->setStyleSheet(QString("QLabel{font-size:16px;font-weight:bold;color:%1;"
        "background:transparent;border:none;}").arg(scCol));
    m_errorsLabel->setText(QString::fromUtf8("ATTT Grade: %1  \xc2\xb7  Errors: %2  \xc2\xb7  Stalls: %3")
        .arg(atttGrade).arg(m_nbErrors).arg(m_nbCalages));

    m_nextBtn->setVisible(false);
    m_endBtn->setVisible(false);
    m_prevBtn->setVisible(false);
    m_examTimerLabel->setVisible(false);

    // Refresh dashboard from DB-sourced counters, then rebuild session history
    updateDashboard();
    refreshHistoryUI();

    // m_totalDepense already reloaded from DB by loadStatsFromDB()
    if(m_depenseLabel)
        m_depenseLabel->setText(QString::fromUtf8(
            "\xf0\x9f\x92\xb0  Total spent: %1 DT").arg(m_totalDepense, 0, 'f', 1));

    // Check exam readiness after each session
    checkExamReadiness();

    // ── Full Exam: chain to next maneuver or show final summary ──
    if (m_fullExamMode) {
        m_fullExamScores.append(score);
        m_fullExamSuccess.append(success);
        if (m_fullExamPhase < 3) {
            m_fullExamPhase++;
            static const int FULL_EXAM_SEQ[] = {5, 0, 4, 3};
            QString nextName;
            switch(FULL_EXAM_SEQ[m_fullExamPhase]){
                case 5: nextName = QString::fromUtf8("Forward Corridor"); break;
                case 0: nextName = QString::fromUtf8("Cr\xc3\xa9neau"); break;
                case 4: nextName = QString::fromUtf8("Demi-tour"); break;
                case 3: nextName = QString::fromUtf8("Marche arri\xc3\xa8re"); break;
                default: nextName = "Next"; break;
            }
            showFloatingMessage(
                QString::fromUtf8("\xf0\x9f\x8e\xaf Phase %1/4 done! \xe2\x86\x92 %2")
                    .arg(m_fullExamPhase).arg(nextName), "#6c5ce7");
            QTimer::singleShot(1800, this, [this](){
                static const int seq[] = {5, 0, 4, 3};
                openManeuver(seq[m_fullExamPhase]);
            });
        } else {
            m_fullExamMode = false;
            m_fullExamPhase = 0;
            showFullExamResults();
        }
        return;  // skip normal single-maneuver results dialog
    }

    // Show results dialog
    showResultsDialog(success, score);
}

// ═══════════════════════════════════════════════════════════════════
//  RESULTS DIALOG — Detailed score breakdown after session
// ═══════════════════════════════════════════════════════════════════

void ParkingWidget::showResultsDialog(bool success, int score)
{
    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("Session Results"));
    dlg.setFixedWidth(440);
    dlg.setStyleSheet("QDialog{background:white;border-radius:16px;}"
        "QLabel{background:transparent;border:none;}");

    QVBoxLayout *l = new QVBoxLayout(&dlg);
    l->setContentsMargins(28,24,28,24);
    l->setSpacing(14);

    // ── Big Result ──
    QString resultIcon = success ?
        QString::fromUtf8("\xf0\x9f\x8e\x89") :
        QString::fromUtf8("\xf0\x9f\x92\xaa");
    QLabel *bigIcon = new QLabel(resultIcon, &dlg);
    bigIcon->setAlignment(Qt::AlignCenter);
    bigIcon->setStyleSheet("QLabel{font-size:52px;}");
    l->addWidget(bigIcon);

    QString resultText = success ?
        QString::fromUtf8("Maneuver passed!") :
        QString::fromUtf8("Pas encore... Recommencez !");
    QLabel *resultLabel = new QLabel(resultText, &dlg);
    resultLabel->setAlignment(Qt::AlignCenter);
    resultLabel->setStyleSheet(QString("QLabel{font-size:20px;font-weight:bold;color:%1;}")
        .arg(success ? "#00b894" : "#e17055"));
    l->addWidget(resultLabel);

    // ── Score ──
    QString atttGrade = score >= 80 ? "A" : score >= 50 ? "B" : "C";
    QString gradeColor = score >= 80 ? "#00b894" : score >= 50 ? "#fdcb6e" : "#e17055";
    QLabel *scoreLabel = new QLabel(QString("Score: %1/100  —  ATTT Grade: %2").arg(score).arg(atttGrade), &dlg);
    scoreLabel->setAlignment(Qt::AlignCenter);
    scoreLabel->setStyleSheet(QString("QLabel{font-size:16px;font-weight:bold;color:%1;}").arg(gradeColor));
    l->addWidget(scoreLabel);

    // ── Score Breakdown ──
    QFrame *breakdown = new QFrame(&dlg);
    breakdown->setObjectName("bd");
    breakdown->setStyleSheet("QFrame#bd{background:#f8f8f8;border-radius:12px;border:none;}");
    QVBoxLayout *bdl = new QVBoxLayout(breakdown);
    bdl->setContentsMargins(18,14,18,14);
    bdl->setSpacing(6);

    QLabel *bdTitle = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x93\x8a  Score breakdown"), breakdown);
    bdTitle->setStyleSheet("QLabel{font-size:13px;font-weight:bold;color:#2d3436;}");
    bdl->addWidget(bdTitle);

    auto addRow = [&](const QString &label, const QString &val, const QString &color){
        QHBoxLayout *r = new QHBoxLayout();
        QLabel *lbl = new QLabel(label, breakdown);
        lbl->setStyleSheet("QLabel{font-size:11px;color:#636e72;}");
        r->addWidget(lbl, 1);
        QLabel *v = new QLabel(val, breakdown);
        v->setStyleSheet(QString("QLabel{font-size:11px;font-weight:bold;color:%1;}").arg(color));
        r->addWidget(v);
        bdl->addLayout(r);
    };

    int mn = m_sessionSeconds/60, sc = m_sessionSeconds%60;
    addRow(QString::fromUtf8("Duration"),
           QString("%1:%2").arg(mn,2,10,QChar('0')).arg(sc,2,10,QChar('0')),
           m_sessionSeconds <= 180 ? "#00b894" : "#e17055");
    addRow(QString::fromUtf8("Steps completed"),
           QString("%1/%2").arg(qMin(m_currentStep,m_totalSteps)).arg(m_totalSteps),
           m_currentStep >= m_totalSteps ? "#00b894" : "#fdcb6e");
    addRow(QString::fromUtf8("Errors"),
           QString::number(m_nbErrors), m_nbErrors == 0 ? "#00b894" : "#e17055");
    addRow(QString::fromUtf8("Stalls"),
           QString("%1 (max 1 tolerated)").arg(m_nbCalages),
           m_nbCalages < 2 ? "#00b894" : "#e17055");
    addRow(QString::fromUtf8("Contact obstacle"),
           m_contactObstacle ? QString::fromUtf8("YES \xe2\x80\x94 Eliminatory") :
                               QString::fromUtf8("Non"),
           m_contactObstacle ? "#d63031" : "#00b894");

    if(m_examMode){
        addRow(QString::fromUtf8("Mode examen (3 min)"),
               m_sessionSeconds <= 180 ?
                   QString::fromUtf8("\xe2\x9c\x85 Within time") :
                   QString::fromUtf8("\xe2\x9d\x8c Time exceeded"),
               m_sessionSeconds <= 180 ? "#00b894" : "#e17055");
    }

    l->addWidget(breakdown);

    // ── XP + Cost ──
    int xpGained = success ? (m_examMode ? 50 : 25) : 10;
    QLabel *xpLabel = new QLabel(QString::fromUtf8(
        "\xe2\xad\x90  +%1 XP   |   Niveau %2   |   %3 XP total")
        .arg(xpGained).arg(m_currentLevel).arg(m_totalXP), &dlg);
    xpLabel->setAlignment(Qt::AlignCenter);
    xpLabel->setStyleSheet("QLabel{font-size:12px;font-weight:bold;color:#00b894;"
        "background:#e8f8f5;border-radius:10px;padding:8px 14px;}");
    l->addWidget(xpLabel);

    QLabel *costLabel = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x92\xb0  Session cost: %1 DT  |  "
        "\xf0\x9f\x9a\x97 %2  |  Total spent: %3 DT")
        .arg(m_selectedTarif, 0, 'f', 1)
        .arg(m_selectedModel)
        .arg(m_totalDepense, 0, 'f', 1), &dlg);
    costLabel->setAlignment(Qt::AlignCenter);
    costLabel->setWordWrap(true);
    costLabel->setStyleSheet("QLabel{font-size:11px;font-weight:bold;color:#00b894;"
        "background:#e8f8f5;border-radius:10px;padding:8px 14px;}");
    l->addWidget(costLabel);

    // ── Advice ──
    QFrame *adviceBox = new QFrame(&dlg);
    adviceBox->setObjectName("adv");
    adviceBox->setStyleSheet("QFrame#adv{background:#f0faf7;border-radius:10px;"
        "border:1px solid #d1f2eb;}");
    QVBoxLayout *advL = new QVBoxLayout(adviceBox);
    advL->setContentsMargins(14,10,14,10);
    advL->setSpacing(4);

    QLabel *advTitle = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x92\xa1  Tip pour la prochaine fois"), adviceBox);
    advTitle->setStyleSheet("QLabel{font-size:11px;font-weight:bold;color:#00b894;}");
    advL->addWidget(advTitle);

    QString advice;
    if(m_contactObstacle)
        advice = QString::fromUtf8("Top priority: watch the distance sensors! "
            "Go slower and stop as soon as the beep accelerates.");
    else if(m_nbCalages >= 2)
        advice = QString::fromUtf8("Work on the biting point. Hold the clutch at the biting point. "
            "and release very gradually.");
    else if(m_sessionSeconds > 180 && m_examMode)
        advice = QString::fromUtf8("Practice in normal mode first to gain fluidity, "
            "puis repassez en mode examen.");
    else if(m_currentStep < m_totalSteps)
        advice = QString::fromUtf8("You have not completed all steps. "
            "Take time to follow each instruction carefully.");
    else if(success && score >= 80)
        advice = QString::fromUtf8("Excellent! Try exam mode now to simulate "
            "real ATTT course conditions.");
    else
        advice = QString::fromUtf8("Good job! Keep practicing regularly "
            "to improve your time and reduce errors.");

    QLabel *advText = new QLabel(advice, adviceBox);
    advText->setWordWrap(true);
    advText->setStyleSheet("QLabel{font-size:11px;color:#636e72;line-height:1.4;}");
    advL->addWidget(advText);
    l->addWidget(adviceBox);

    // ── Buttons ──
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(12);

    QPushButton *retryBtn = new QPushButton(QString::fromUtf8(
        "\xf0\x9f\x94\x84  Recommencer"), &dlg);
    retryBtn->setFixedHeight(42);
    retryBtn->setCursor(Qt::PointingHandCursor);
    retryBtn->setStyleSheet("QPushButton{background:#00b894;color:white;border:none;border-radius:12px;"
        "font-size:13px;font-weight:bold;padding:0 20px;}"
        "QPushButton:hover{background:#009874;}");
    connect(retryBtn, &QPushButton::clicked, &dlg, [this,&dlg](){
        dlg.accept();
        openManeuver(m_currentManeuver);
    });
    btnRow->addWidget(retryBtn, 1);

    QPushButton *homeBtn = new QPushButton(QString::fromUtf8(
        "\xf0\x9f\x8f\xa0  Accueil"), &dlg);
    homeBtn->setFixedHeight(42);
    homeBtn->setCursor(Qt::PointingHandCursor);
    homeBtn->setStyleSheet("QPushButton{background:#f0f2f5;color:#636e72;border:none;border-radius:14px;"
        "font-size:13px;font-weight:bold;padding:0 20px;}"
        "QPushButton:hover{background:#dfe6e9;}");
    connect(homeBtn, &QPushButton::clicked, &dlg, [this,&dlg](){
        dlg.accept();
        m_stack->setCurrentIndex(1);
    });
    btnRow->addWidget(homeBtn, 1);

    l->addLayout(btnRow);

    dlg.exec();
}

// ═══════════════════════════════════════════════════════════════════
//  FULL EXAM — open, chain, results
// ═══════════════════════════════════════════════════════════════════

void ParkingWidget::openFullExam()
{
    m_fullExamMode = true;
    m_fullExamPhase = 0;
    m_fullExamScores.clear();
    m_fullExamSuccess.clear();
    // Sequence: Marche avant (5) → Créneau (0) → Demi-tour (4) → Marche arrière (3)
    static const int FULL_EXAM_SEQ[] = {5, 0, 4, 3};
    openManeuver(FULL_EXAM_SEQ[0]);
}

QWidget* ParkingWidget::createFullExamCard()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("feCard");
    card->setStyleSheet(
        "QFrame#feCard{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "stop:0 #6c5ce7,stop:1 #a29bfe);"
        "border-radius:20px;border:none;}"
    );
    card->setGraphicsEffect(shadow(card, 24, 22));
    card->setMinimumHeight(160);

    QVBoxLayout *l = new QVBoxLayout(card);
    l->setContentsMargins(24, 20, 24, 20);
    l->setSpacing(10);

    // Title row
    QHBoxLayout *titleRow = new QHBoxLayout();
    titleRow->setSpacing(14);

    QLabel *trophyIc = new QLabel(QString::fromUtf8("\xf0\x9f\x8f\x86"), card);
    trophyIc->setFixedSize(52, 52);
    trophyIc->setAlignment(Qt::AlignCenter);
    trophyIc->setStyleSheet("QLabel{font-size:28px;background:rgba(255,255,255,0.18);"
        "border-radius:14px;border:none;}");
    titleRow->addWidget(trophyIc);

    QVBoxLayout *titleText = new QVBoxLayout();
    titleText->setSpacing(3);
    QLabel *title = new QLabel(QString::fromUtf8("Full ATTT Exam"), card);
    title->setStyleSheet("QLabel{font-size:18px;font-weight:bold;color:white;"
        "background:transparent;border:none;}");
    titleText->addWidget(title);
    QLabel *sub = new QLabel(QString::fromUtf8(
        "4 maneuvers in sequence — real exam conditions"), card);
    sub->setStyleSheet("QLabel{font-size:10px;color:rgba(255,255,255,0.8);"
        "background:transparent;border:none;}");
    titleText->addWidget(sub);
    titleRow->addLayout(titleText, 1);

    QLabel *badge = new QLabel(QString::fromUtf8("ATTT"), card);
    badge->setStyleSheet("QLabel{font-size:10px;font-weight:bold;color:#6c5ce7;"
        "background:white;border-radius:10px;padding:4px 12px;border:none;}");
    titleRow->addWidget(badge, 0, Qt::AlignTop);

    l->addLayout(titleRow);

    // 4-step sequence pills
    QHBoxLayout *seqRow = new QHBoxLayout();
    seqRow->setSpacing(8);
    QStringList phases = {
        QString::fromUtf8("\xe2\xac\x86\xef\xb8\x8f Fwd Corridor"),
        QString::fromUtf8("\xf0\x9f\x85\xbf\xef\xb8\x8f Cr\xc3\xa9neau"),
        QString::fromUtf8("\xf0\x9f\x94\x84 Demi-tour"),
        QString::fromUtf8("\xe2\xac\x87\xef\xb8\x8f Marche AR")
    };
    for(int i = 0; i < phases.size(); i++){
        QLabel *pill = new QLabel(QString("%1. %2").arg(i+1).arg(phases[i]), card);
        pill->setStyleSheet("QLabel{font-size:9px;font-weight:bold;color:#6c5ce7;"
            "background:white;border-radius:10px;padding:4px 10px;border:none;}");
        seqRow->addWidget(pill, 1, Qt::AlignLeft);
    }
    seqRow->addStretch();
    l->addLayout(seqRow);

    // Start button
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    QPushButton *startBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x9a\x80  Start Full Exam  \xe2\x86\x92"), card);
    startBtn->setFixedHeight(42);
    startBtn->setCursor(Qt::PointingHandCursor);
    startBtn->setStyleSheet(
        "QPushButton{background:white;color:#6c5ce7;border:none;"
        "border-radius:12px;font-size:13px;font-weight:bold;padding:0 28px;}"
        "QPushButton:hover{background:#f0f0ff;}");
    connect(startBtn, &QPushButton::clicked, this, &ParkingWidget::openFullExam);
    btnRow->addWidget(startBtn);
    l->addLayout(btnRow);

    return card;
}

void ParkingWidget::showFullExamResults()
{
    // Build combined score
    int totalScore = 0;
    int passed = 0;
    for(int i = 0; i < m_fullExamScores.size(); i++){
        totalScore += m_fullExamScores[i];
        if(m_fullExamSuccess.value(i, false)) passed++;
    }
    int combined = m_fullExamScores.isEmpty() ? 0 : totalScore / m_fullExamScores.size();
    bool overallPass = (passed == 4);

    // Save combined result to DB
    ParkingDBManager::instance().saveResultatExamen(
        m_currentEleveId, "full_exam",
        m_sessionSeconds, overallPass,
        combined * 20.0 / 100.0,
        m_nbErrors, overallPass ? "" : "At least one maneuver failed", "");

    QDialog dlg(this);
    dlg.setWindowTitle(QString::fromUtf8("Full Exam Results"));
    dlg.setFixedWidth(460);
    dlg.setStyleSheet("QDialog{background:white;border-radius:16px;}"
        "QLabel{background:transparent;border:none;}");

    QVBoxLayout *l = new QVBoxLayout(&dlg);
    l->setContentsMargins(28, 24, 28, 24);
    l->setSpacing(14);

    // Big result icon
    QLabel *bigIcon = new QLabel(overallPass ?
        QString::fromUtf8("\xf0\x9f\x8f\x86") :
        QString::fromUtf8("\xf0\x9f\x92\xaa"), &dlg);
    bigIcon->setAlignment(Qt::AlignCenter);
    bigIcon->setStyleSheet("QLabel{font-size:56px;}");
    l->addWidget(bigIcon);

    QLabel *resultLbl = new QLabel(overallPass ?
        QString::fromUtf8("Full Exam PASSED!") :
        QString::fromUtf8("Not yet — keep training!"), &dlg);
    resultLbl->setAlignment(Qt::AlignCenter);
    resultLbl->setStyleSheet(QString("QLabel{font-size:20px;font-weight:bold;color:%1;}")
        .arg(overallPass ? "#6c5ce7" : "#e17055"));
    l->addWidget(resultLbl);

    QLabel *scoreLbl = new QLabel(
        QString("Combined score: %1/100  —  %2/4 maneuvers passed").arg(combined).arg(passed), &dlg);
    scoreLbl->setAlignment(Qt::AlignCenter);
    scoreLbl->setStyleSheet(QString("QLabel{font-size:14px;font-weight:bold;color:%1;}")
        .arg(combined >= 80 ? "#6c5ce7" : combined >= 50 ? "#fdcb6e" : "#e17055"));
    l->addWidget(scoreLbl);

    // Per-phase breakdown
    QFrame *breakdown = new QFrame(&dlg);
    breakdown->setObjectName("feb");
    breakdown->setStyleSheet("QFrame#feb{background:#f8f8f8;border-radius:12px;border:none;}");
    QVBoxLayout *bdl = new QVBoxLayout(breakdown);
    bdl->setContentsMargins(18, 14, 18, 14);
    bdl->setSpacing(8);

    QLabel *bdTitle = new QLabel(QString::fromUtf8("\xf0\x9f\x93\x8a  Maneuver breakdown"), breakdown);
    bdTitle->setStyleSheet("QLabel{font-size:13px;font-weight:bold;color:#2d3436;}");
    bdl->addWidget(bdTitle);

    static const int FULL_EXAM_SEQ[] = {5, 0, 4, 3};
    QStringList phaseNames = {
        QString::fromUtf8("\xe2\xac\x86\xef\xb8\x8f Forward Corridor"),
        QString::fromUtf8("\xf0\x9f\x85\xbf\xef\xb8\x8f Cr\xc3\xa9neau (Parallel)"),
        QString::fromUtf8("\xf0\x9f\x94\x84 Demi-tour (U-turn)"),
        QString::fromUtf8("\xe2\xac\x87\xef\xb8\x8f Marche arri\xc3\xa8re (Reverse)")
    };
    for(int i = 0; i < 4 && i < m_fullExamScores.size(); i++){
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *nm = new QLabel(phaseNames[i], breakdown);
        nm->setStyleSheet("QLabel{font-size:11px;color:#2d3436;}");
        row->addWidget(nm, 1);

        bool ok = m_fullExamSuccess.value(i, false);
        int sc = m_fullExamScores.value(i, 0);
        QLabel *res = new QLabel(
            QString("%1  %2/100").arg(ok ?
                QString::fromUtf8("\xe2\x9c\x85") :
                QString::fromUtf8("\xe2\x9d\x8c")).arg(sc), breakdown);
        res->setStyleSheet(QString("QLabel{font-size:11px;font-weight:bold;color:%1;}")
            .arg(ok ? "#00b894" : "#e17055"));
        row->addWidget(res);
        bdl->addLayout(row);
    }
    l->addWidget(breakdown);

    // Advice
    QLabel *advLbl = new QLabel(overallPass ?
        QString::fromUtf8("\xf0\x9f\x8e\x89  Excellent! You are ready for the real ATTT exam. "
            "Book your exam slot now.") :
        QString::fromUtf8("\xf0\x9f\x92\xa1  Focus on the maneuvers you failed. "
            "Practice each one individually, then retry the Full Exam."), &dlg);
    advLbl->setWordWrap(true);
    advLbl->setStyleSheet("QLabel{font-size:11px;color:#636e72;background:#f0faf7;"
        "border-radius:10px;padding:10px 14px;}");
    l->addWidget(advLbl);

    // Buttons
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(12);

    QPushButton *retryBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x94\x84  Retry Full Exam"), &dlg);
    retryBtn->setFixedHeight(44);
    retryBtn->setCursor(Qt::PointingHandCursor);
    retryBtn->setStyleSheet("QPushButton{background:#6c5ce7;color:white;border:none;"
        "border-radius:12px;font-size:13px;font-weight:bold;padding:0 20px;}"
        "QPushButton:hover{background:#5a4bd1;}");
    connect(retryBtn, &QPushButton::clicked, &dlg, [this, &dlg](){
        dlg.accept();
        openFullExam();
    });
    btnRow->addWidget(retryBtn, 1);

    QPushButton *homeBtn = new QPushButton(
        QString::fromUtf8("\xf0\x9f\x8f\xa0  Home"), &dlg);
    homeBtn->setFixedHeight(44);
    homeBtn->setCursor(Qt::PointingHandCursor);
    homeBtn->setStyleSheet("QPushButton{background:#f0f2f5;color:#636e72;border:none;"
        "border-radius:14px;font-size:13px;font-weight:bold;padding:0 20px;}"
        "QPushButton:hover{background:#dfe6e9;}");
    connect(homeBtn, &QPushButton::clicked, &dlg, [this, &dlg](){
        dlg.accept();
        m_stack->setCurrentIndex(1);
    });
    btnRow->addWidget(homeBtn, 1);

    l->addLayout(btnRow);
    dlg.exec();
}

// ═══════════════════════════════════════════════════════════════════
//  MARCHE AVANT SENSOR HANDLER — forward corridor sensor events
// ═══════════════════════════════════════════════════════════════════

void ParkingWidget::handleMarcheAvantSensorEvent(const QString &event)
{
    const auto &steps = m_allManeuvers[5];
    if (m_currentStep >= steps.size()) return;
    const QString &phase = steps[m_currentStep].sensorPhase;

    // PREP / DONE — no automatic action
    if (phase == "PREP" || phase == "DONE") return;

    // Side obstacle warnings — both applicable in all driving phases
    if (event == "LEFT_OBSTACLE") {
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xe2\x9a\xa0 LEFT barrier too close! Steer gently RIGHT."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#e17055;font-weight:bold;"
            "background:#fdf3f0;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
        flashSensorZone(m_zLeft, "#e17055");
        speak(QString::fromUtf8("Left barrier close. Steer right."));
        recordError("LEFT_BARRIER_CLOSE");
    }
    else if (event == "RIGHT_OBSTACLE") {
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xe2\x9a\xa0 RIGHT barrier too close! Steer gently LEFT."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#e17055;font-weight:bold;"
            "background:#fdf3f0;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
        flashSensorZone(m_zRight, "#e17055");
        speak(QString::fromUtf8("Right barrier close. Steer left."));
        recordError("RIGHT_BARRIER_CLOSE");
    }
    // Front sensor triggers — approaching end of corridor
    else if (event == "FRONT_SENSOR1" || event == "FRONT_OBSTACLE") {
        if (phase == "DRIVE" || phase == "MIDWAY") {
            m_guidanceMsg->setText(QString::fromUtf8(
                "\xf0\x9f\x9b\x91 Slow down — approaching end of corridor."));
            m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#fdcb6e;font-weight:bold;"
                "background:#fef9e7;border-radius:10px;padding:10px 14px;border:none;}");
            m_guidanceMsg->setVisible(true);
            flashSensorZone(m_zFront, "#fdcb6e");
        }
    }
    else if (event == "FRONT_SENSOR2") {
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xf0\x9f\x9b\x91 STOP — exit line reached!"));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#00b894;font-weight:bold;"
            "background:#e8f8f5;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
        flashSensorZone(m_zFront, "#00b894");
        speak(QString::fromUtf8("Stop. Exit line reached."));
    }
    // Contact: both side sensors simultaneously → corridor contact
    else if ((event == "LEFT_SENSOR1" && phase == "ALIGN") ||
             (event == "RIGHT_SENSOR1" && phase == "ALIGN")) {
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xf0\x9f\x9f\xa2 Aligned with corridor centerline. Drive forward."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#00b894;font-weight:bold;"
            "background:#e8f8f5;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
    }
}

int ParkingWidget::calculateScore(bool success)
{
    int score = 100;
    if(!success) score -= 30;
    score -= m_nbErrors * 10;
    score -= m_nbCalages * 15;
    if(m_contactObstacle) score -= 40;
    if(m_examMode && m_sessionSeconds > 180) score -= 20;
    // Bonus for speed in exam mode
    if(m_examMode && success && m_sessionSeconds <= 120) score += 5;
    return qMax(0, qMin(100, score));
}


// ═══════════════════════════════════════════════════════════════════
//  ERROR RECORDING
// ═══════════════════════════════════════════════════════════════════

void ParkingWidget::recordCalage()
{
    if(!m_sessionActive) return;
    m_nbCalages++;
    updateLiveScore();

    if(m_nbCalages >= 2){
        showFloatingMessage(QString::fromUtf8(
            "\xf0\x9f\x9a\xab 2 stalls = ELIMINATORY at the exam !"), "#d63031");
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xe2\x9a\xa0\xef\xb8\x8f  2 stalls: ATTT eliminatory fault!"));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#d63031;font-weight:bold;"
            "background:#fef0f0;border-radius:10px;padding:10px 14px;border:none;}");
        speak(QString::fromUtf8("Warning! Second stall. Eliminatory fault at the exam."));
    } else {
        showFloatingMessage(QString::fromUtf8(
            "\xf0\x9f\x9a\xab Stall recorded (%1/1 tolerated)")
            .arg(m_nbCalages), "#e17055");
        speak(QString::fromUtf8("Stall recorded. One stall tolerated."));
    }
    m_guidanceMsg->setVisible(true);

    // Log to DB
    ParkingDBManager::instance().logSensorEvent(m_currentSessionId,
        0,0,0,0,0, -1,-1,-1,-1,-1,
        "STALL", QString("Stall #%1").arg(m_nbCalages), m_currentStep);
}

void ParkingWidget::recordObstacleContact()
{
    if(!m_sessionActive) return;
    m_contactObstacle = true;
    m_nbErrors++;
    updateLiveScore();

    showFloatingMessage(QString::fromUtf8(
        "\xf0\x9f\x92\xa5 OBSTACLE CONTACT — ELIMINATORY !"), "#d63031");
    m_guidanceMsg->setText(QString::fromUtf8(
        "\xf0\x9f\x92\xa5  Obstacle contact! Eliminatory fault at ATTT exam."));
    m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#d63031;font-weight:bold;"
        "background:#fef0f0;border-radius:10px;padding:10px 14px;border:2px solid #fab1a0;}");
    m_guidanceMsg->setVisible(true);

    speak(QString::fromUtf8("Obstacle contact! Eliminatory fault. Stop the vehicle."));

    ParkingDBManager::instance().logSensorEvent(m_currentSessionId,
        0,0,0,0,0, -1,-1,-1,-1,-1,
        "CONTACT_OBSTACLE", "Contact obstacle", m_currentStep);
}

void ParkingWidget::recordError(const QString &type)
{
    if(!m_sessionActive) return;
    m_nbErrors++;
    updateLiveScore();

    showFloatingMessage(QString::fromUtf8(
        "\xe2\x9a\xa0\xef\xb8\x8f Error recorded (%1)").arg(m_nbErrors), "#fdcb6e");

    ParkingDBManager::instance().logSensorEvent(m_currentSessionId,
        0,0,0,0,0, -1,-1,-1,-1,-1,
        "ERROR_" + type, QString("Error #%1").arg(m_nbErrors), m_currentStep);
}

void ParkingWidget::updateLiveScore()
{
    if(m_liveScoreLabel){
        m_liveScoreLabel->setText(QString::fromUtf8(
            "Stalls: %1  \xc2\xb7  Contacts: %2  \xc2\xb7  Errors: %3")
            .arg(m_nbCalages).arg(m_contactObstacle ? 1 : 0).arg(m_nbErrors));

        // Color based on severity
        QString col = "#b2bec3";
        if(m_contactObstacle || m_nbCalages >= 2) col = "#d63031";
        else if(m_nbErrors > 0 || m_nbCalages > 0) col = "#e17055";
        m_liveScoreLabel->setStyleSheet(
            QString("QLabel{font-size:10px;color:%1;background:transparent;border:none;font-weight:bold;}").arg(col));
    }

    // Also update session status card
    m_errorsLabel->setText(QString::fromUtf8(
        "Errors: %1  \xc2\xb7  Stalls: %2%3")
        .arg(m_nbErrors).arg(m_nbCalages)
        .arg(m_contactObstacle ? QString::fromUtf8("  \xc2\xb7  \xf0\x9f\x92\xa5 CONTACT") : ""));
}

// ═══════════════════════════════════════════════════════════════════
//  FLOATING MESSAGE
// ═══════════════════════════════════════════════════════════════════

void ParkingWidget::showFloatingMessage(const QString &msg, const QString &bgColor)
{
    if(!m_floatingMsg) return;
    m_floatingMsg->setText(msg);
    m_floatingMsg->setStyleSheet(QString(
        "QLabel{font-size:13px;font-weight:bold;color:white;"
        "background:%1;border-radius:12px;padding:8px 24px;border:none;}").arg(bgColor));
    m_floatingMsg->setVisible(true);
    m_floatingMsg->raise();

    // Position at top center of session page
    QWidget *parent = m_floatingMsg->parentWidget();
    if(parent){
        int x = (parent->width() - m_floatingMsg->sizeHint().width()) / 2;
        m_floatingMsg->move(qMax(10,x), 60);
        m_floatingMsg->adjustSize();
    }

    // Auto hide
    m_floatingTimer->stop();
    m_floatingTimer->start(2500);
}

// ═══════════════════════════════════════════════════════════════════
//  FLOATING NOTES BUBBLE — User notepad (WhatsApp/Snapchat style)
// ═══════════════════════════════════════════════════════════════════

QString ParkingWidget::quickNotePath() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + "/wino_notes.json";
}

void ParkingWidget::toggleNotesBubble()
{
    m_notesBubbleOpen = !m_notesBubbleOpen;
    m_notesBubble->setVisible(m_notesBubbleOpen);
    if(m_notesBubbleOpen) {
        // Update title with current maneuver context
        if(m_sessionActive && m_currentManeuver >= 0 && m_currentManeuver < m_maneuverNames.size()) {
            m_notesBubbleTitle->setText(QString::fromUtf8("\xf0\x9f\x93\x9d %1").arg(m_maneuverNames[m_currentManeuver]));
        } else {
            m_notesBubbleTitle->setText(QString::fromUtf8("\xf0\x9f\x93\x9d Mes Notes"));
        }
        m_notesTextEdit->setFocus();
        repositionNotesBubble();
    }
    // Toggle FAB icon
    m_notesFAB->setText(m_notesBubbleOpen ?
        QString::fromUtf8("\xe2\x9c\x95") : QString::fromUtf8("\xf0\x9f\x93\x9d"));
}

void ParkingWidget::repositionNotesBubble()
{
    // ── Reposition Stats FAB on vehicle selection page ──
    if(m_vehStatsFAB && m_vehPage) {
        int margin = 16;
        m_vehStatsFAB->move(m_vehPage->width()  - m_vehStatsFAB->width()  - margin,
                            m_vehPage->height() - m_vehStatsFAB->height() - margin);
        m_vehStatsFAB->raise();
    }

    if(!m_notesFAB || !m_notesBubbleParent) return;
    QWidget *p = m_notesBubbleParent;
    int margin = 16;
    int fabH   = m_notesFAB->height();   // 56 px

    // Notes FAB: bottom-right corner
    m_notesFAB->move(p->width() - m_notesFAB->width() - margin,
                     p->height() - fabH - margin);
    m_notesFAB->raise();

    // Stats FAB: directly above the notes FAB (same X, 10 px gap)
    if(m_statsFAB) {
        m_statsFAB->move(p->width() - m_statsFAB->width() - margin,
                         p->height() - fabH - margin - fabH - 10);
        m_statsFAB->raise();
    }

    // Bubble: above the FAB, aligned right
    if(m_notesBubble) {
        int bx = p->width() - m_notesBubble->width() - margin;
        int by = p->height() - m_notesFAB->height() - m_notesBubble->height() - margin - 10;
        m_notesBubble->move(qMax(10, bx), qMax(60, by));
        m_notesBubble->raise();
    }
}

void ParkingWidget::saveQuickNote()
{
    if(!m_notesTextEdit) return;
    QString content = m_notesTextEdit->toPlainText().trimmed();
    if(content.isEmpty()) return;

    // Read existing notes
    QString path = quickNotePath();
    QJsonArray arr;
    QFile fileR(path);
    if(fileR.open(QIODevice::ReadOnly)) {
        arr = QJsonDocument::fromJson(fileR.readAll()).array();
        fileR.close();
    }

    // Find existing parking quick-note or create new
    QString noteId = "parking_quick_note";
    bool found = false;
    for(int i = 0; i < arr.size(); i++) {
        QJsonObject obj = arr[i].toObject();
        if(obj["id"].toString() == noteId) {
            obj["content"] = content;
            obj["updatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            if(m_sessionActive && m_currentManeuver >= 0 && m_currentManeuver < m_maneuverNames.size())
                obj["title"] = QString("Parking - %1").arg(m_maneuverNames[m_currentManeuver]);
            arr[i] = obj;
            found = true;
            break;
        }
    }
    if(!found) {
        QJsonObject obj;
        obj["id"] = noteId;
        obj["title"] = (m_sessionActive && m_currentManeuver >= 0 && m_currentManeuver < m_maneuverNames.size())
            ? QString("Parking - %1").arg(m_maneuverNames[m_currentManeuver])
            : "Parking Notes";
        obj["content"] = content;
        obj["category"] = "lesson";
        obj["color"] = "#d3f9d8";
        obj["createdAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        obj["updatedAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        obj["pinned"] = false;
        obj["archived"] = false;
        arr.append(obj);
    }

    // Write back
    QFile fileW(path);
    if(fileW.open(QIODevice::WriteOnly)) {
        fileW.write(QJsonDocument(arr).toJson(QJsonDocument::Compact));
        fileW.close();
    }

    // Update status
    m_notesSaveStatus->setText(QString::fromUtf8("\xe2\x9c\x93 Saved"));
    m_notesSaveStatus->setStyleSheet("QLabel{font-size:9px;color:rgba(255,255,255,0.7);"
        "background:transparent;border:none;}");
}

void ParkingWidget::loadQuickNote()
{
    if(!m_notesTextEdit) return;
    QString path = quickNotePath();
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)) return;

    QJsonArray arr = QJsonDocument::fromJson(file.readAll()).array();
    file.close();

    for(const auto &val : arr) {
        QJsonObject obj = val.toObject();
        if(obj["id"].toString() == "parking_quick_note") {
            m_notesTextEdit->blockSignals(true);
            m_notesTextEdit->setPlainText(obj["content"].toString());
            m_notesTextEdit->blockSignals(false);
            break;
        }
    }
}

void ParkingWidget::flashSensorZone(QLabel *zone, const QString &color)
{
    if(!zone) return;
    QString origSS = zone->styleSheet();
    zone->setStyleSheet(QString("QLabel{background:%1;border-radius:6px;border:2px solid %1;}").arg(color));
    QTimer::singleShot(400, this, [zone, origSS](){
        zone->setStyleSheet(origSS);
    });
}


// ═══════════════════════════════════════════════════════════════════
//  UI UPDATE HELPERS
// ═══════════════════════════════════════════════════════════════════

void ParkingWidget::updateStepUI()
{
    if(m_totalSteps == 0) return;
    const auto &step = m_allManeuvers[m_currentManeuver][m_currentStep];
    m_stepNum->setText(QString::number(m_currentStep + 1));
    m_stepNum->setStyleSheet(QString("QLabel{background:%1;color:white;font-size:24px;font-weight:bold;"
        "border-radius:28px;border:3px solid %1;}").arg(m_maneuverColors[m_currentManeuver]));
    m_stepTitle->setText(step.title);
    m_stepInstruction->setText(step.instruction);
    m_stepTip->setText(step.detail);
    m_stepSensor->setText(step.assistMsg);
    if (m_stepDiagram)
        m_stepDiagram->setStep(m_currentManeuver, m_currentStep, m_totalSteps);

    // Reset DB message label for this new step
    if (m_dbMsgLabel) {
        m_dbMsgLabel->setVisible(false);
        m_dbMsgLabel->setStyleSheet(
            "QLabel{background:#fff3cd;color:#856404;font-size:12px;font-weight:bold;"
            "border-radius:10px;padding:10px 14px;"
            "border-left:4px solid #ffc107;border-top:none;border-right:none;border-bottom:none;}");
    }

    // Reposition notes FAB
    repositionNotesBubble();
}

void ParkingWidget::updateDots()
{
    // Clear existing dots
    QLayoutItem *item;
    while((item = m_dotsLayout->takeAt(0)) != nullptr){
        if(item->widget()) delete item->widget();
        delete item;
    }
    m_dots.clear();

    for(int i = 0; i < m_totalSteps; i++){
        QWidget *dot = new QWidget(this);
        if(i < m_currentStep){
            dot->setFixedSize(24, 8);
            dot->setStyleSheet("background:#00b894;border-radius:4px;border:none;");
        } else if(i == m_currentStep){
            dot->setFixedSize(32, 8);
            dot->setStyleSheet(QString("background:%1;border-radius:4px;border:none;")
                .arg(m_maneuverColors[m_currentManeuver]));
        } else {
            dot->setFixedSize(24, 8);
            dot->setStyleSheet("background:#dfe6e9;border-radius:4px;border:none;");
        }
        m_dotsLayout->addWidget(dot);
        m_dots.append(dot);
    }
    m_dotsLayout->addStretch();

    QLabel *stepLbl = new QLabel(QString::fromUtf8("Step %1/%2")
        .arg(m_currentStep+1).arg(m_totalSteps), this);
    stepLbl->setStyleSheet(QString("QLabel{font-size:10px;font-weight:bold;color:%1;"
        "background:transparent;border:none;}").arg(m_maneuverColors[m_currentManeuver]));
    m_dotsLayout->addWidget(stepLbl);
}

void ParkingWidget::updateCommonMistakes()
{
    if(m_currentManeuver < 0 || m_currentManeuver >= m_allMistakes.size()) return;
    if(!m_mistakeTitle || !m_mistakeDesc || !m_mistakeTip) return;
    const auto &mistakes = m_allMistakes[m_currentManeuver];
    int idx = qMin(m_currentStep, mistakes.size() - 1);
    if(idx < 0) return;
    const auto &mk = mistakes[idx];
    m_mistakeTitle->setText(QString::fromUtf8("\xe2\x9a\xa0 ") + mk.mistake);
    m_mistakeDesc->setText(QString::fromUtf8("\xe2\x9d\x8c ") + mk.consequence);
    m_mistakeTip->setText(QString::fromUtf8("\xf0\x9f\x92\xa1 ") + mk.tip);
    m_mistakeTip->setVisible(true);
}

void ParkingWidget::updateSessionTimer()
{
    m_sessionSeconds++;
    int mn = m_sessionSeconds / 60, sc = m_sessionSeconds % 60;
    m_timerLabel->setText(QString("%1:%2").arg(mn,2,10,QChar('0')).arg(sc,2,10,QChar('0')));

    // Color timer based on duration
    if(m_sessionSeconds > 150 && m_examMode){
        m_timerLabel->setStyleSheet("QLabel{font-size:22px;font-weight:bold;color:#e17055;"
            "font-family:'Courier New',monospace;background:#fef3e2;border-radius:12px;padding:4px 16px;border:none;}");
    }

    if(m_examMode){
        int rem = 180 - m_sessionSeconds;
        if(rem < 0) rem = 0;
        int rm = rem / 60, rs = rem % 60;
        m_examTimerLabel->setText(QString::fromUtf8("\xe2\x8f\xb1 %1:%2")
            .arg(rm,2,10,QChar('0')).arg(rs,2,10,QChar('0')));
        m_examTimerLabel->setVisible(true);

        if(rem <= 30){
            m_examTimerLabel->setStyleSheet("QLabel{font-size:14px;font-weight:bold;color:#d63031;"
                "font-family:'Courier New',monospace;background:#fef0f0;border-radius:8px;padding:2px 8px;border:none;}");
            if(rem == 30){
                showFloatingMessage(QString::fromUtf8(
                    "\xe2\x8f\xb1 30 seconds remaining !"), "#e17055");
                speak(QString::fromUtf8("Warning, 30 seconds remaining."));
            }
            if(rem == 10)
                speak(QString::fromUtf8("10 seconds!"));
        } else {
            m_examTimerLabel->setStyleSheet("QLabel{font-size:14px;font-weight:bold;color:#e17055;"
                "font-family:'Courier New',monospace;background:transparent;border:none;}");
            if(rem == 60)
                speak(QString::fromUtf8("One minute remaining."));
        }

        if(rem <= 0) endSession();
    }
}

void ParkingWidget::updateDashboard()
{
    QStringList lvlNames = {QString::fromUtf8("Beginner"), QString::fromUtf8("Apprentice"),
        QString::fromUtf8("Confirmed"), "Expert", QString::fromUtf8("Master")};
    QStringList lvlIcons = {QString::fromUtf8("\xf0\x9f\x8c\xb1"), QString::fromUtf8("\xf0\x9f\x8c\xbf"),
        QString::fromUtf8("\xf0\x9f\x8c\xb3"), QString::fromUtf8("\xe2\xad\x90"), QString::fromUtf8("\xf0\x9f\x91\x91")};
    int li = qMax(0, qMin(m_currentLevel - 1, 4));
    m_dashLevel->setText(QString("%1 Lv. %2 — %3").arg(lvlIcons[li]).arg(m_currentLevel).arg(lvlNames[li]));
    m_dashXPBar->setValue(m_totalXP % 100);
    m_dashXPLabel->setText(QString("%1 XP").arg(m_totalXP % 100));
    m_statSessions->setText(QString::number(m_totalSessionsCount));
    int rate = m_totalSessionsCount > 0 ? (m_totalSuccessCount * 100 / m_totalSessionsCount) : 0;
    m_statSuccess->setText(QString("%1%").arg(rate));
    if(m_bestTimeSeconds < 999){
        int bm = m_bestTimeSeconds/60, bs = m_bestTimeSeconds%60;
        m_statBest->setText(QString("%1:%2").arg(bm,2,10,QChar('0')).arg(bs,2,10,QChar('0')));
    }
    m_statStreak->setText(QString::number(m_practiceStreak));

    for(int i = 0; i < 5 && i < m_masteryBars.size(); i++){
        int p = m_maneuverAttemptCounts[i] > 0 ?
            (m_maneuverSuccessCounts[i]*100/m_maneuverAttemptCounts[i]) : 0;
        m_masteryBars[i]->setValue(p);
        m_masteryPcts[i]->setText(QString("%1%").arg(p));
    }

    // Achievements
    QStringList badgeIcons = {QString::fromUtf8("\xf0\x9f\x8c\xb1"), QString::fromUtf8("\xf0\x9f\x94\xb2"),
        QString::fromUtf8("\xe2\x9a\xa1"), QString::fromUtf8("\xf0\x9f\x94\xa5"),
        QString::fromUtf8("\xf0\x9f\x8e\xaf"), QString::fromUtf8("\xf0\x9f\x91\x91")};
    QList<bool> unlocked = {
        m_totalSessionsCount >= 1,
        m_maneuverSuccessCounts[0] >= 1,
        m_bestTimeSeconds < 120,
        m_practiceStreak >= 3,
        (m_totalSuccessCount > 0 && m_nbErrors == 0 && !m_contactObstacle),
        (m_maneuverSuccessCounts[0]>0 && m_maneuverSuccessCounts[1]>0 &&
         m_maneuverSuccessCounts[2]>0 && m_maneuverSuccessCounts[3]>0 &&
         m_maneuverSuccessCounts[4]>0)
    };
    for(int i = 0; i < 6 && i < m_achieveIcons.size(); i++){
        if(unlocked[i]){
            m_achieveIcons[i]->setText(badgeIcons[i]);
            m_achieveIcons[i]->setStyleSheet("QLabel{font-size:18px;background:#e8f8f5;border-radius:18px;border:none;}");
        }
    }
}


// ═══════════════════════════════════════════════════════════════════
//  SENSOR MANAGEMENT
// ═══════════════════════════════════════════════════════════════════

void ParkingWidget::connectSensorSignals()
{
    connect(m_sensorMgr, &SensorManager::connected, this, [this](){ onConnectionChanged(true); });
    connect(m_sensorMgr, &SensorManager::disconnected, this, [this](){ onConnectionChanged(false); });
    connect(m_sensorMgr, &SensorManager::rightObstacleDetected, this, [this](){ onSensorEvent("RIGHT_OBSTACLE"); });
    connect(m_sensorMgr, &SensorManager::leftObstacleDetected, this, [this](){ onSensorEvent("LEFT_OBSTACLE"); });
    connect(m_sensorMgr, &SensorManager::leftSensor1Detected, this, [this](){ onSensorEvent("LEFT_SENSOR1"); });
    connect(m_sensorMgr, &SensorManager::rightSensor2Detected, this, [this](){ onSensorEvent("RIGHT_SENSOR2"); });
    connect(m_sensorMgr, &SensorManager::rearRightSensorDetected, this, [this](){ onSensorEvent("REAR_RIGHT_SENSOR"); });
    connect(m_sensorMgr, &SensorManager::bothRearObstacleDetected, this, [this](){ onSensorEvent("BOTH_REAR_OBSTACLE"); });
    connect(m_sensorMgr, &SensorManager::bothRearSensorDetected, this, [this](){ onSensorEvent("BOTH_REAR_SENSOR"); });
    connect(m_sensorMgr, &SensorManager::sensorDataUpdated, this, [this](const ParkingSensorData &d){
        if(d.distAvant >= 0)     onDistanceUpdated("front", (int)d.distAvant);
        if(d.distGauche >= 0)    onDistanceUpdated("left", (int)d.distGauche);
        if(d.distDroit >= 0)     onDistanceUpdated("right", (int)d.distDroit);
        if(d.distArrGauche >= 0) onDistanceUpdated("rear_left", (int)d.distArrGauche);
        if(d.distArrDroit >= 0)  onDistanceUpdated("rear_right", (int)d.distArrDroit);
    });
}

void ParkingWidget::connectMaquette()
{
    QStringList ports = SensorManager::availablePorts();
    if(ports.isEmpty()){
        m_connStatus->setText(QString::fromUtf8("\xf0\x9f\x94\xb4 No port"));
        m_connStatus->setStyleSheet("QLabel{font-size:9px;color:#e17055;background:#fef3e2;"
            "border-radius:8px;padding:3px 8px;border:none;}");
        return;
    }
    bool ok = m_sensorMgr->connectToPort(ports.first());
    if(ok){
        m_maquetteConnected = true;
        onConnectionChanged(true);
    } else {
        m_connStatus->setText(QString::fromUtf8("\xf0\x9f\x94\xb4 Failed"));
        m_connStatus->setStyleSheet("QLabel{font-size:9px;color:#e17055;background:#fef3e2;"
            "border-radius:8px;padding:3px 8px;border:none;}");
    }
}

void ParkingWidget::onConnectionChanged(bool connected)
{
    m_maquetteConnected = connected;
    if(connected){
        m_connStatus->setText(QString::fromUtf8("\xf0\x9f\x9f\xa2 Connected"));
        m_connStatus->setStyleSheet("QLabel{font-size:9px;color:#00b894;background:#e8f8f5;"
            "border-radius:8px;padding:3px 8px;border:none;}");
        showFloatingMessage(QString::fromUtf8(
            "\xf0\x9f\x9f\xa2 Model connected!"), "#00b894");
    } else {
        m_connStatus->setText(QString::fromUtf8("\xf0\x9f\x94\xb4 Disconnected"));
        m_connStatus->setStyleSheet("QLabel{font-size:9px;color:#e17055;background:#fef3e2;"
            "border-radius:8px;padding:3px 8px;border:none;}");
    }
}

void ParkingWidget::onSensorEvent(const QString &event)
{
    if(!m_sessionActive) return;

    // Update sensor display
    m_stepSensor->setText(QString::fromUtf8("\xf0\x9f\x93\xa1 Sensor: %1").arg(event));

    // Show DB guidance message (from PARKING_MANEUVER_STEPS.guidance_message)
    if (m_dbMsgLabel) {
        QString dbMsg = dbMessageForEvent(event);
        if (!dbMsg.isEmpty()) {
            m_dbMsgLabel->setText(dbMsg);
            m_dbMsgLabel->setVisible(true);
        }
    }

    // Auto-increment errors based on sensor event vs expected DB step
    checkSensorErrors(event);

    // Log sensor event
    ParkingDBManager::instance().logSensorEvent(m_currentSessionId,
        0,0,0,0,0, m_lastDistFront, m_lastDistLeft, m_lastDistRight,
        m_lastDistRearL, m_lastDistRearR,
        event, "", m_currentStep);

    // Route: Real Exam Mode vs Step-by-Step mode
    if (m_examMode && m_realExamPhase != EXAM_IDLE) {
        handleRealExamEvent(event);
    } else {
        handleSensorEventForManeuver(event);
    }
}

void ParkingWidget::handleSensorEventForManeuver(const QString &event)
{
    switch(m_currentManeuver){
    case 0: handleCreneauSensorEvent(event); break;
    case 1: handleBatailleSensorEvent(event); break;
    case 2: handleEpiSensorEvent(event); break;
    case 3: handleMarcheArriereSensorEvent(event); break;
    case 4: handleDemiTourSensorEvent(event); break;
    case 5: handleMarcheAvantSensorEvent(event); break;
    }
}

void ParkingWidget::handleCreneauSensorEvent(const QString &event)
{
    const auto &steps = m_allManeuvers[0];
    if(m_currentStep >= steps.size()) return;
    const QString &phase = steps[m_currentStep].sensorPhase;

    // ── Step 0: PREP — safety checks (no sensor trigger, manual advance) ──
    if(phase == "PREP") {
        // No automatic sensor action — user must manually advance after visual checks
        return;
    }

    // ── Step 1: ALIGN — lateral distance confirmed by right sensor ──
    if(phase == "ALIGN" && (event == "RIGHT_OBSTACLE" || event == "RIGHT_SENSOR1")){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xf0\x9f\x93\x8f Alignment confirmed! Mirror \xe2\x87\x94 rear bumper. Ready to steer."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#00b894;font-weight:bold;"
            "background:#e8f8f5;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
        flashSensorZone(m_zRight, "#00b894");
    }

    // ── Step 2: TURN_RIGHT — angle building, left sensor detects progress ──
    else if(phase == "TURN_RIGHT" && event == "LEFT_SENSOR1"){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xf0\x9f\x94\x84 Good angle! Approaching 45\xc2\xb0 — prepare to STOP."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#0984e3;font-weight:bold;"
            "background:#e3f2fd;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
        flashSensorZone(m_zLeft, "#0984e3");
    }
    // Warn if too close to right during turn
    else if(phase == "TURN_RIGHT" && event == "RIGHT_OBSTACLE"){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xe2\x9a\xa0 Right side close! Slow down, watch your clearance."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#e17055;font-weight:bold;"
            "background:#fef3e2;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
        flashSensorZone(m_zRight, "#e17055");
    }

    // ── Step 3: CHECK_ANGLE — 45° confirmed, must stop ──
    else if(phase == "CHECK_ANGLE" && event == "LEFT_SENSOR1"){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xe2\x9b\x94 45\xc2\xb0 reached! STOP now — prepare to counter-steer LEFT."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#e17055;font-weight:bold;"
            "background:#fef3e2;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
        flashSensorZone(m_zLeft, "#fdcb6e");
        showFloatingMessage(QString::fromUtf8("\xe2\x9b\x94 STOP \xe2\x80\x94 45\xc2\xb0 !"), "#e17055");
    }

    // ── Step 4: TURN_LEFT — counter-steer, rear sensors monitor ──
    else if(phase == "TURN_LEFT" && (event == "BOTH_REAR_OBSTACLE" || event == "BOTH_REAR_SENSOR")){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xe2\x9c\x85 Rear clear! Continue reversing — almost parallel."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#00b894;font-weight:bold;"
            "background:#e8f8f5;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
        flashSensorZone(m_zRearL, "#00b894");
        flashSensorZone(m_zRearR, "#00b894");
    }
    else if(phase == "TURN_LEFT" && event == "RIGHT_SENSOR2"){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xe2\x9a\xa0 Right sensor: rear car nearby — slow down!"));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#fdcb6e;font-weight:bold;"
            "background:#fef9e7;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
        flashSensorZone(m_zRight, "#fdcb6e");
    }

    // ── Step 5: STRAIGHT — alignment check ──
    else if(phase == "STRAIGHT" && event == "RIGHT_SENSOR2"){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xe2\x9c\x85 Sensors confirm parallel alignment! Adjust to 20-30 cm."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#00b894;font-weight:bold;"
            "background:#e8f8f5;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
        flashSensorZone(m_zRight, "#00b894");
    }

    // ── Step 6: DONE — no sensor action needed ──
    // (manual advance to complete)
}

void ParkingWidget::handleBatailleSensorEvent(const QString &event)
{
    const auto &steps = m_allManeuvers[1];
    if(m_currentStep >= steps.size()) return;
    const QString &phase = steps[m_currentStep].sensorPhase;

    if(phase == "DETECT" && (event == "RIGHT_OBSTACLE" || event == "LEFT_OBSTACLE")){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xf0\x9f\x93\x8f Space detected! Start steering."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#00b894;font-weight:bold;"
            "background:#e8f8f5;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
    } else if(phase == "TURN" && event == "REAR_RIGHT_SENSOR"){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xf0\x9f\x94\x84 Bon angle ! Redressez les roues."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#0984e3;font-weight:bold;"
            "background:#e3f2fd;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
    } else if(phase == "CENTER" && (event == "BOTH_REAR_OBSTACLE" || event == "BOTH_REAR_SENSOR")){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xe2\x9c\x85 Stop! You are at the back of the space."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#00b894;font-weight:bold;"
            "background:#e8f8f5;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
        flashSensorZone(m_zRearL, "#00b894");
        flashSensorZone(m_zRearR, "#00b894");
    }
}

void ParkingWidget::handleEpiSensorEvent(const QString &event)
{
    const auto &steps = m_allManeuvers[2];
    if(m_currentStep >= steps.size()) return;
    const QString &phase = steps[m_currentStep].sensorPhase;

    if(phase == "DETECT" && (event == "RIGHT_OBSTACLE" || event == "LEFT_OBSTACLE")){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xf0\x9f\x93\x8f Diagonal space found! Advance to steering point."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#0984e3;font-weight:bold;"
            "background:#e3f2fd;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
    } else if(phase == "TURN" && event == "LEFT_SENSOR1"){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xf0\x9f\x94\x84 Steering point atteint ! Tournez vers la place."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#00b894;font-weight:bold;"
            "background:#e8f8f5;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
    } else if(phase == "ENTER" && event == "RIGHT_SENSOR2"){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xe2\x9a\xa0 Obstacle nearby, slow down!"));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#e17055;font-weight:bold;"
            "background:#fef3e2;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
        flashSensorZone(m_zRight, "#e17055");
    }
}

void ParkingWidget::handleMarcheArriereSensorEvent(const QString &event)
{
    const auto &steps = m_allManeuvers[3];
    if(m_currentStep >= steps.size()) return;
    const QString &phase = steps[m_currentStep].sensorPhase;

    if(phase == "PREP" && (event == "BOTH_REAR_OBSTACLE" || event == "BOTH_REAR_SENSOR")){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xe2\x9a\xa0 Obstacle detected behind! Check before reversing."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#e17055;font-weight:bold;"
            "background:#fef3e2;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
        flashSensorZone(m_zRearL, "#e17055");
        flashSensorZone(m_zRearR, "#e17055");
    } else if(phase == "REVERSE" && (event == "LEFT_OBSTACLE" || event == "RIGHT_OBSTACLE")){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xf0\x9f\x94\x84 Deviation detected! Correct the trajectory."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#fdcb6e;font-weight:bold;"
            "background:#fef9e7;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
    } else if(phase == "CORRECT"){
        if(event == "LEFT_SENSOR1" || event == "RIGHT_SENSOR2"){
            m_guidanceMsg->setText(QString::fromUtf8(
                "\xe2\x9c\x85 Trajectory corrected! Continue straight."));
            m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#00b894;font-weight:bold;"
                "background:#e8f8f5;border-radius:10px;padding:10px 14px;border:none;}");
            m_guidanceMsg->setVisible(true);
        }
    }
}

void ParkingWidget::handleDemiTourSensorEvent(const QString &event)
{
    const auto &steps = m_allManeuvers[4];
    if(m_currentStep >= steps.size()) return;
    const QString &phase = steps[m_currentStep].sensorPhase;

    if(phase == "FORWARD" && (event == "RIGHT_OBSTACLE" || event == "LEFT_OBSTACLE")){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xe2\x9a\xa0 Curb nearby! Stop and shift to reverse."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#e17055;font-weight:bold;"
            "background:#fef3e2;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
        showFloatingMessage(QString::fromUtf8("\xe2\x9b\x94 STOP — Trottoir !"), "#e17055");
    } else if(phase == "REVERSE" && (event == "BOTH_REAR_OBSTACLE" || event == "BOTH_REAR_SENSOR")){
        m_guidanceMsg->setText(QString::fromUtf8(
            "\xe2\x9a\xa0 Rear curb detected! Stop and drive forward."));
        m_guidanceMsg->setStyleSheet("QLabel{font-size:12px;color:#e17055;font-weight:bold;"
            "background:#fef3e2;border-radius:10px;padding:10px 14px;border:none;}");
        m_guidanceMsg->setVisible(true);
        flashSensorZone(m_zRearL, "#e17055");
        flashSensorZone(m_zRearR, "#e17055");
        showFloatingMessage(QString::fromUtf8("\xe2\x9b\x94 STOP — Rear curb!"), "#e17055");
    }
}

void ParkingWidget::updateCarSensorZone(QLabel *zone, QLabel *distLabel, int distCm)
{
    QString col;
    if(distCm < 30) col = "#e17055";
    else if(distCm < 80) col = "#fdcb6e";
    else col = "#00b894";
    zone->setStyleSheet(QString("QLabel{background:%1;border-radius:6px;border:none;}").arg(col));
    distLabel->setText(QString("%1cm").arg(distCm));

    // Flash if too close
    if(distCm < 20 && m_sessionActive){
        flashSensorZone(zone, "#d63031");
        if(distCm < 10){
            showFloatingMessage(QString::fromUtf8(
                "\xf0\x9f\x9a\xa8 VERY CLOSE ! %1 cm").arg(distCm), "#d63031");
        }
    }
}

void ParkingWidget::onDistanceUpdated(const QString &sensor, int distCm)
{
    if(sensor.contains("front", Qt::CaseInsensitive)){
        updateCarSensorZone(m_zFront, m_dFront, distCm);
        if(distCm < m_lastDistFront || m_lastDistFront < 0)
            speakSensorWarning(QString::fromUtf8("front"), distCm);
        m_lastDistFront = distCm;
    } else if(sensor.contains("left", Qt::CaseInsensitive) && !sensor.contains("rear")){
        updateCarSensorZone(m_zLeft, m_dLeft, distCm);
        if(distCm < m_lastDistLeft || m_lastDistLeft < 0)
            speakSensorWarning(QString::fromUtf8("left side"), distCm);
        m_lastDistLeft = distCm;
    } else if(sensor.contains("right", Qt::CaseInsensitive) && !sensor.contains("rear")){
        updateCarSensorZone(m_zRight, m_dRight, distCm);
        if(distCm < m_lastDistRight || m_lastDistRight < 0)
            speakSensorWarning(QString::fromUtf8("right side"), distCm);
        m_lastDistRight = distCm;
    } else if(sensor.contains("rear") && sensor.contains("left")){
        updateCarSensorZone(m_zRearL, m_dRearL, distCm);
        if(distCm < m_lastDistRearL || m_lastDistRearL < 0)
            speakSensorWarning(QString::fromUtf8("rear left"), distCm);
        m_lastDistRearL = distCm;
    } else if(sensor.contains("rear")){
        updateCarSensorZone(m_zRearR, m_dRearR, distCm);
        if(distCm < m_lastDistRearR || m_lastDistRearR < 0)
            speakSensorWarning(QString::fromUtf8("rear right"), distCm);
        m_lastDistRearR = distCm;
    }
}

void ParkingWidget::onDelayedAction()
{
    if(m_pendingStep >= 0 && m_pendingStep < m_totalSteps){
        m_currentStep = m_pendingStep;
        updateStepUI();
        updateDots();
        updateCommonMistakes();
        m_pendingStep = -1;
    }
}

// ═══════════════════════════════════════════════════════════════════
//  EXAM READINESS CARD
// ═══════════════════════════════════════════════════════════════════

QWidget* ParkingWidget::createExamReadinessCard()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("readyC");
    card->setStyleSheet(cardSS("readyC"));
    card->setGraphicsEffect(shadow(card));
    QVBoxLayout *l = new QVBoxLayout(card);
    l->setContentsMargins(20,16,20,16);
    l->setSpacing(12);

    // Title
    QLabel *title = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x8e\x93  ATTT Exam Eligibility"), card);
    title->setStyleSheet("QLabel{font-size:16px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
    l->addWidget(title);

    // Readiness status
    m_examReadyLabel = new QLabel("", card);
    m_examReadyLabel->setWordWrap(true);
    m_examReadyLabel->setStyleSheet("QLabel{font-size:13px;color:#636e72;background:#f8f8f8;"
        "border-radius:12px;padding:12px 16px;border:none;}");
    l->addWidget(m_examReadyLabel);

    // Criteria detail
    QHBoxLayout *critRow = new QHBoxLayout();
    critRow->setSpacing(12);

    // Criterion 1: Hours
    QFrame *c1 = new QFrame(card);
    c1->setObjectName("c1f");
    c1->setStyleSheet("QFrame#c1f{background:#e8f8f5;border-radius:12px;border:none;}");
    QVBoxLayout *c1l = new QVBoxLayout(c1);
    c1l->setContentsMargins(14,10,14,10);
    c1l->setSpacing(4);
    QLabel *c1t = new QLabel(QString::fromUtf8("\xe2\x8f\xb1  Criterion 1"), c1);
    c1t->setStyleSheet("QLabel{font-size:10px;font-weight:bold;color:#00b894;background:transparent;border:none;}");
    c1l->addWidget(c1t);
    auto &db = ParkingDBManager::instance();
    int totalSec = db.getTotalPracticeSeconds(m_currentEleveId);
    int hours = totalSec / 3600;
    int mins = (totalSec % 3600) / 60;
    QLabel *c1v = new QLabel(QString::fromUtf8(
        "%1h %2min / 2h minimum").arg(hours).arg(mins), c1);
    c1v->setStyleSheet("QLabel{font-size:11px;color:#2d3436;background:transparent;border:none;}");
    c1l->addWidget(c1v);
    QProgressBar *c1bar = new QProgressBar(c1);
    c1bar->setRange(0, 120); // 2h in minutes
    c1bar->setValue(qMin(totalSec/60, 120));
    c1bar->setFixedHeight(8);
    c1bar->setTextVisible(false);
    c1bar->setStyleSheet("QProgressBar{background:#d1f2eb;border:none;border-radius:4px;}"
        "QProgressBar::chunk{background:#00b894;border-radius:4px;}");
    c1l->addWidget(c1bar);
    critRow->addWidget(c1, 1);

    // Criterion 2: Perfect runs
    QFrame *c2 = new QFrame(card);
    c2->setObjectName("c2f");
    c2->setStyleSheet("QFrame#c2f{background:#e8f8f5;border-radius:12px;border:none;}");
    QVBoxLayout *c2l = new QVBoxLayout(c2);
    c2l->setContentsMargins(14,10,14,10);
    c2l->setSpacing(4);
    QLabel *c2t = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x8e\xaf  Criterion 2"), c2);
    c2t->setStyleSheet("QLabel{font-size:10px;font-weight:bold;color:#00b894;background:transparent;border:none;}");
    c2l->addWidget(c2t);
    QLabel *c2desc = new QLabel(QString::fromUtf8(
        "3 perfect runs / maneuver"), c2);
    c2desc->setStyleSheet("QLabel{font-size:10px;color:#636e72;background:transparent;border:none;}");
    c2l->addWidget(c2desc);
    for(int i = 0; i < 5 && i < m_maneuverDbKeys.size(); i++){
        int perfect = db.getPerfectRunsCount(m_currentEleveId, m_maneuverDbKeys[i]);
        QHBoxLayout *pr = new QHBoxLayout();
        pr->setSpacing(4);
        QLabel *pi = new QLabel(m_maneuverIcons[i] + " " + m_maneuverNames[i], c2);
        pi->setStyleSheet("QLabel{font-size:9px;color:#2d3436;background:transparent;border:none;}");
        pr->addWidget(pi, 1);
        QLabel *pv = new QLabel(QString("%1/3 %2")
            .arg(qMin(perfect, 3))
            .arg(perfect >= 3 ? QString::fromUtf8("\xe2\x9c\x85") : ""), c2);
        pv->setStyleSheet(QString("QLabel{font-size:9px;font-weight:bold;color:%1;"
            "background:transparent;border:none;}").arg(perfect >= 3 ? "#00b894" : "#636e72"));
        pr->addWidget(pv);
        c2l->addLayout(pr);
    }
    critRow->addWidget(c2, 1);

    l->addLayout(critRow);

    // ── Finance row ──
    QHBoxLayout *finRow = new QHBoxLayout();
    finRow->setSpacing(12);
    m_depenseLabel = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x92\xb0  Total spent: %1 DT").arg(m_totalDepense, 0, 'f', 1), card);
    m_depenseLabel->setStyleSheet("QLabel{font-size:12px;font-weight:bold;color:#e17055;"
        "background:#fef3e2;border-radius:10px;padding:8px 14px;border:none;}");
    finRow->addWidget(m_depenseLabel, 1);

    QLabel *tarifInfo = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x9a\x97  Tarif actuel : %1 DT / session").arg(m_selectedTarif, 0, 'f', 1), card);
    tarifInfo->setStyleSheet("QLabel{font-size:11px;color:#636e72;background:#f8f8f8;"
        "border-radius:10px;padding:8px 14px;border:none;}");
    finRow->addWidget(tarifInfo);
    l->addLayout(finRow);

    // ── Reservation button (visible only when ready) ──
    m_reservationBtn = new QPushButton(QString::fromUtf8(
        "\xf0\x9f\x93\x85  Book an exam slot  \xe2\x86\x92"), card);
    m_reservationBtn->setFixedHeight(48);
    m_reservationBtn->setCursor(Qt::PointingHandCursor);
    m_reservationBtn->setStyleSheet("QPushButton{background:#00b894;color:white;border:none;border-radius:12px;"
        "font-size:14px;font-weight:bold;}"
        "QPushButton:hover{background:#009874;}");
    m_reservationBtn->setVisible(false);
    connect(m_reservationBtn, &QPushButton::clicked, this, [this](){
        m_stack->setCurrentIndex(3);
    });
    l->addWidget(m_reservationBtn);

    return card;
}

void ParkingWidget::checkExamReadiness()
{
    auto &db = ParkingDBManager::instance();
    // Only the 5 standard maneuvers matter for exam eligibility (not marche_avant at index 5)
    m_examReady = db.isExamReady(m_currentEleveId, m_maneuverDbKeys.mid(0, 5));

    if(m_examReadyLabel){
        if(m_examReady){
            m_examReadyLabel->setText(QString::fromUtf8(
                "\xe2\x9c\x85  Congratulations! You are eligible to take the ATTT exam!\n"
                "You can now book your exam slot."));
            m_examReadyLabel->setStyleSheet("QLabel{font-size:13px;color:#00b894;font-weight:bold;"
                "background:#e8f8f5;border-radius:12px;padding:12px 16px;border:2px solid #00b894;}");
        } else {
            int totalSec = db.getTotalPracticeSeconds(m_currentEleveId);
            int hours = totalSec / 3600;
            int remainH = qMax(0, 2 - hours);
            m_examReadyLabel->setText(QString::fromUtf8(
                "\xf0\x9f\x94\x92  Not yet eligible. Keep training!\n"
                "You still need about %1h of practice or 3 perfect runs for each maneuver.")
                .arg(remainH));
            m_examReadyLabel->setStyleSheet("QLabel{font-size:13px;color:#636e72;"
                "background:#f8f8f8;border-radius:12px;padding:12px 16px;border:none;}");
        }
    }

    if(m_reservationBtn){
        m_reservationBtn->setVisible(m_examReady);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  RESERVATION PAGE
// ═══════════════════════════════════════════════════════════════════

QWidget* ParkingWidget::createReservationPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName("resvPage");
    page->setStyleSheet(QString("QWidget#resvPage{%1}").arg(BG));

    QVBoxLayout *pL = new QVBoxLayout(page);
    pL->setContentsMargins(0,0,0,0);
    pL->setSpacing(0);

    // Top bar
    QWidget *topBar = new QWidget(this);
    topBar->setObjectName("rTB");
    topBar->setStyleSheet("QWidget#rTB{background:white;border-bottom:1px solid #eaeaea;}");
    topBar->setFixedHeight(52);
    QHBoxLayout *tbL = new QHBoxLayout(topBar);
    tbL->setContentsMargins(20,0,20,0);
    tbL->setSpacing(14);

    QPushButton *back = new QPushButton(QString::fromUtf8("\xe2\x86\x90  Accueil"), topBar);
    back->setCursor(Qt::PointingHandCursor);
    back->setStyleSheet("QPushButton{background:#e8f8f5;color:#00b894;border:none;border-radius:12px;"
        "padding:7px 16px;font-size:12px;font-weight:bold;}QPushButton:hover{background:#d1f2eb;}");
    connect(back, &QPushButton::clicked, this, [this](){ m_stack->setCurrentIndex(1); });
    tbL->addWidget(back);

    QLabel *rTitle = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x93\x85  ATTT Exam Booking"), topBar);
    rTitle->setStyleSheet("QLabel{font-size:15px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
    tbL->addWidget(rTitle, 1);
    pL->addWidget(topBar);

    // Scroll area
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(SCROLL_SS);
    QWidget *content = new QWidget(this);
    content->setStyleSheet("background:transparent;");
    QVBoxLayout *cL = new QVBoxLayout(content);
    cL->setContentsMargins(32,24,32,24);
    cL->setSpacing(20);

    // ── Status ──
    QFrame *statusCard = new QFrame(this);
    statusCard->setObjectName("rsC");
    statusCard->setStyleSheet(cardSS("rsC"));
    statusCard->setGraphicsEffect(shadow(statusCard));
    QVBoxLayout *sl = new QVBoxLayout(statusCard);
    sl->setContentsMargins(20,16,20,16);
    sl->setSpacing(10);

    QLabel *statusTitle = new QLabel(QString::fromUtf8(
        "\xe2\x9c\x85  You are eligible for the exam!"), this);
    statusTitle->setStyleSheet("QLabel{font-size:18px;font-weight:bold;color:#00b894;background:transparent;border:none;}");
    sl->addWidget(statusTitle);

    QLabel *statusDesc = new QLabel(QString::fromUtf8(
        "Book your slot to take the exam de maneuvers at the ATTT course.\n"
        "Choose a date and a available time slot."), this);
    statusDesc->setStyleSheet("QLabel{font-size:12px;color:#636e72;background:transparent;border:none;line-height:1.5;}");
    statusDesc->setWordWrap(true);
    sl->addWidget(statusDesc);
    cL->addWidget(statusCard);

    // ── New Reservation Form ──
    m_newBookingForm = new QFrame(this);
    m_newBookingForm->setObjectName("rfC");
    m_newBookingForm->setStyleSheet(cardSS("rfC"));
    m_newBookingForm->setGraphicsEffect(shadow(m_newBookingForm));
    QVBoxLayout *fl = new QVBoxLayout(m_newBookingForm);
    fl->setContentsMargins(20,16,20,16);
    fl->setSpacing(14);

    QLabel *formTitle = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x93\x9d  New booking"), this);
    formTitle->setStyleSheet("QLabel{font-size:16px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
    fl->addWidget(formTitle);

    // Date selection
    QHBoxLayout *dateRow = new QHBoxLayout();
    dateRow->setSpacing(12);
    QLabel *dateLbl = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x93\x85  Exam date:"), this);
    dateLbl->setStyleSheet("QLabel{font-size:12px;color:#636e72;background:transparent;border:none;}");
    dateRow->addWidget(dateLbl);

    QComboBox *dateCombo = new QComboBox(this);
    dateCombo->setStyleSheet("QComboBox{background:white;border:2px solid #d1f2eb;border-radius:10px;"
        "padding:8px 14px;font-size:12px;color:#2d3436;min-width:180px;}"
        "QComboBox:hover{border-color:#00b894;}"
        "QComboBox::drop-down{border:none;padding-right:10px;}"
        "QComboBox QAbstractItemView{background:white;color:#2d3436;selection-background-color:#00b894;selection-color:white;outline:none;}");
    // Generate next 14 days (weekdays only)
    QDate today = QDate::currentDate();
    for(int d = 1; d <= 21; d++){
        QDate dt = today.addDays(d);
        int dow = dt.dayOfWeek();
        if(dow >= 1 && dow <= 6){ // Lundi-Samedi
            dateCombo->addItem(dt.toString("dddd dd/MM/yyyy"), dt.toString("yyyy-MM-dd"));
        }
    }
    dateRow->addWidget(dateCombo, 1);
    fl->addLayout(dateRow);

    // Creneau selection
    QHBoxLayout *creneauRow = new QHBoxLayout();
    creneauRow->setSpacing(12);
    QLabel *crLbl = new QLabel(QString::fromUtf8(
        "\xe2\x8f\xb0  Parallel :"), this);
    crLbl->setStyleSheet("QLabel{font-size:12px;color:#636e72;background:transparent;border:none;}");
    creneauRow->addWidget(crLbl);

    QComboBox *creneauCombo = new QComboBox(this);
    creneauCombo->setStyleSheet("QComboBox{background:white;border:2px solid #d1f2eb;border-radius:10px;"
        "padding:8px 14px;font-size:12px;color:#2d3436;min-width:180px;}"
        "QComboBox:hover{border-color:#00b894;}"
        "QComboBox::drop-down{border:none;padding-right:10px;}"
        "QComboBox QAbstractItemView{background:white;color:#2d3436;selection-background-color:#00b894;selection-color:white;outline:none;}");
    creneauCombo->addItem(QString::fromUtf8("08:00 - 09:00 (Matin)"), "08:00-09:00");
    creneauCombo->addItem(QString::fromUtf8("09:00 - 10:00 (Matin)"), "09:00-10:00");
    creneauCombo->addItem(QString::fromUtf8("10:00 - 11:00 (Matin)"), "10:00-11:00");
    creneauCombo->addItem(QString::fromUtf8("11:00 - 12:00 (Matin)"), "11:00-12:00");
    creneauCombo->addItem(QString::fromUtf8("14:00 - 15:00 (Afternoon)"), "14:00-15:00");
    creneauCombo->addItem(QString::fromUtf8("15:00 - 16:00 (Afternoon)"), "15:00-16:00");
    creneauCombo->addItem(QString::fromUtf8("16:00 - 17:00 (Afternoon)"), "16:00-17:00");
    creneauRow->addWidget(creneauCombo, 1);
    fl->addLayout(creneauRow);

    // Instructor selection
    QHBoxLayout *instRow = new QHBoxLayout();
    instRow->setSpacing(12);
    QLabel *instLbl = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x91\xa8\xe2\x80\x8d\xe2\x9a\xaa  Instructor :"), this);
    instLbl->setStyleSheet("QLabel{font-size:12px;color:#636e72;background:transparent;border:none;}");
    instRow->addWidget(instLbl);

    QComboBox *instCombo = new QComboBox(this);
    instCombo->setStyleSheet("QComboBox{background:white;border:2px solid #d1f2eb;border-radius:10px;"
        "padding:8px 14px;font-size:12px;color:#2d3436;min-width:180px;}"
        "QComboBox:hover{border-color:#00b894;}"
        "QComboBox::drop-down{border:none;padding-right:10px;}"
        "QComboBox QAbstractItemView{background:white;color:#2d3436;selection-background-color:#00b894;selection-color:white;outline:none;}");
    
    QList<QVariantMap> instructors = ParkingDBManager::instance().getAllMoniteurs();
    for (const auto &inst : instructors) {
        instCombo->addItem(QString("%1 %2").arg(inst["prenom"].toString(), inst["nom"].toString()), inst["id"]);
    }
    instRow->addWidget(instCombo, 1);
    fl->addLayout(instRow);

    // Price info
    QLabel *priceLbl = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x92\xb0  Exam fee : 50.0 DT  \xc2\xb7  "
        "Total spent on training: %1 DT")
        .arg(m_totalDepense, 0, 'f', 1), this);
    priceLbl->setWordWrap(true);
    priceLbl->setStyleSheet("QLabel{font-size:12px;font-weight:bold;color:#e17055;"
        "background:#fef3e2;border-radius:10px;padding:10px 14px;border:none;}");
    fl->addWidget(priceLbl);

    // Submit button
    QPushButton *submitBtn = new QPushButton(QString::fromUtf8(
        "\xe2\x9c\x85  Confirm booking"), this);
    submitBtn->setFixedHeight(48);
    submitBtn->setCursor(Qt::PointingHandCursor);
    submitBtn->setStyleSheet("QPushButton{background:#00b894;color:white;border:none;border-radius:12px;"
        "font-size:14px;font-weight:bold;}"
        "QPushButton:hover{background:#009874;}");
    connect(submitBtn, &QPushButton::clicked, this,
        [this, dateCombo, creneauCombo, instCombo](){
            QString dateStr = dateCombo->currentData().toString();
            QString creneau = creneauCombo->currentData().toString();
            int instId = instCombo->currentData().toInt();
            if(dateStr.isEmpty()){
                QMessageBox::warning(this, "", QString::fromUtf8("Please select a date."));
                return;
            }
            if(instId <= 0){
                QMessageBox::warning(this, "", QString::fromUtf8("Please select an instructor."));
                return;
            }
            int resId = ParkingDBManager::instance().addReservationExamen(
                m_currentEleveId, instId, m_currentVehiculeId, dateStr, creneau, 50.0);
            if(resId > 0){
                QMessageBox::information(this, QString::fromUtf8("Booking confirmed"),
                    QString::fromUtf8("\xe2\x9c\x85 Your exam is booked!\n\n"
                        "\xf0\x9f\x93\x85 Date : %1\n"
                        "\xe2\x8f\xb0 Parallel : %2\n"
                        "\xf0\x9f\x92\xb0 Montant : 50 DT\n\n"
                        "Arrive 15 min before your time.\n"
                        "Bonne chance \xf0\x9f\x8d\x80 !")
                    .arg(dateCombo->currentText(), creneauCombo->currentText()));
                // Refresh reservations list
                showReservationDialog();
            } else {
                QMessageBox::warning(this, "", QString::fromUtf8("Error during booking."));
            }
        });
    fl->addWidget(submitBtn);

    cL->addWidget(m_newBookingForm);

    // ── Existing Reservations ──
    QFrame *listCard = new QFrame(this);
    listCard->setObjectName("rlC");
    listCard->setStyleSheet(cardSS("rlC"));
    listCard->setGraphicsEffect(shadow(listCard));
    QVBoxLayout *ll = new QVBoxLayout(listCard);
    ll->setContentsMargins(20,16,20,16);
    ll->setSpacing(10);

    QLabel *listTitle = new QLabel(QString::fromUtf8(
        "\xf0\x9f\x93\x9c  My bookings"), this);
    listTitle->setStyleSheet("QLabel{font-size:14px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
    ll->addWidget(listTitle);

    m_reservationsLayout = new QVBoxLayout();
    m_reservationsLayout->setSpacing(8);
    ll->addLayout(m_reservationsLayout);
    cL->addWidget(listCard);

    // Load existing reservations
    showReservationDialog();

    cL->addStretch();
    scroll->setWidget(content);
    pL->addWidget(scroll, 1);
    return page;
}

void ParkingWidget::showReservationDialog()
{
    if(!m_reservationsLayout) return;

    // Clear existing
    QLayoutItem *item;
    while((item = m_reservationsLayout->takeAt(0)) != nullptr){
        if(item->widget()) delete item->widget();
        delete item;
    }

    auto reservations = ParkingDBManager::instance().getReservationsEleve(m_currentEleveId);
    
    // Check for active bookings to restrict new ones
    bool hasActiveBooking = false;
    for(const auto &r : reservations){
        QString statut = r["statut"].toString();
        if(statut == "APPROVED" || statut == "confirmee" || statut == "PENDING" || statut == "en_attente") {
            hasActiveBooking = true;
            break;
        }
    }
    
    if (m_newBookingForm) {
        m_newBookingForm->setVisible(!hasActiveBooking);
    }

    if(reservations.isEmpty()){
        QLabel *empty = new QLabel(QString::fromUtf8(
            "\xf0\x9f\x93\xad  No reservations.\nBook your first exam slot !"), this);
        empty->setAlignment(Qt::AlignCenter);
        empty->setStyleSheet("QLabel{font-size:11px;color:#b2bec3;background:#f8f8f8;"
            "border-radius:10px;padding:16px;border:none;}");
        m_reservationsLayout->addWidget(empty);
        return;
    }

    for(const auto &r : reservations){
        QWidget *row = new QWidget(this);
        row->setStyleSheet("background:transparent;");
        QHBoxLayout *rl = new QHBoxLayout(row);
        rl->setContentsMargins(10,8,10,8);
        rl->setSpacing(10);

        int resId = r["id"].toInt();
        QString statut = r["statut"].toString();
        QString dateEx = r["date_examen"].toString();
        QString creneau = r["creneau"].toString();
        double montant = r["montant_paye"].toDouble();
        QString modele = r.contains("modele") ? r["modele"].toString() : "";

        // Status icon
        QString stIcon = (statut == "APPROVED" || statut == "confirmee") ? QString::fromUtf8("\xe2\x9c\x85") :
                          (statut == "CANCELLED" || statut == "annulee") ? QString::fromUtf8("\xe2\x9d\x8c") :
                          QString::fromUtf8("\xe2\x8f\xb3");
        QLabel *stLabel = new QLabel(stIcon, row);
        stLabel->setFixedSize(24, 24);
        stLabel->setAlignment(Qt::AlignCenter);
        stLabel->setStyleSheet("QLabel{font-size:14px;background:transparent;border:none;}");
        rl->addWidget(stLabel);

        // Details
        QVBoxLayout *detCol = new QVBoxLayout();
        detCol->setSpacing(2);
        QLabel *dateLbl = new QLabel(QString::fromUtf8(
            "\xf0\x9f\x93\x85 %1  \xe2\x80\x94  \xe2\x8f\xb0 %2").arg(dateEx, creneau), row);
        dateLbl->setStyleSheet("QLabel{font-size:11px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
        detCol->addWidget(dateLbl);
        if(!modele.isEmpty()){
            QLabel *vehLbl = new QLabel(QString::fromUtf8(
                "\xf0\x9f\x9a\x97 %1").arg(modele), row);
            vehLbl->setStyleSheet("QLabel{font-size:9px;color:#636e72;background:transparent;border:none;}");
            detCol->addWidget(vehLbl);
        }
        rl->addLayout(detCol, 1);

        // Amount
        QLabel *amtLbl = new QLabel(QString("%1 DT").arg(montant, 0, 'f', 1), row);
        amtLbl->setStyleSheet("QLabel{font-size:11px;font-weight:bold;color:#e17055;"
            "background:#fef3e2;border-radius:8px;padding:4px 10px;border:none;}");
        rl->addWidget(amtLbl);

        // Cancel button (only for PENDING)
        if (statut == "PENDING" || statut == "en_attente") {
            QPushButton *cancelBtn = new QPushButton(QString::fromUtf8("Cancel"), row);
            cancelBtn->setCursor(Qt::PointingHandCursor);
            cancelBtn->setStyleSheet("QPushButton{background:#ffeaa7;color:#d63031;border:none;border-radius:8px;"
                "font-size:9px;font-weight:bold;padding:4px 10px;}"
                "QPushButton:hover{background:#fdcb6e;}");
            connect(cancelBtn, &QPushButton::clicked, this, [this, resId](){
                if(QMessageBox::question(this, "Cancel Booking", 
                    QString::fromUtf8("Are you sure you want to cancel your exam booking?")) == QMessageBox::Yes){
                    ParkingDBManager::instance().updateReservationStatut(resId, "CANCELLED");
                    showReservationDialog(); // Refresh
                }
            });
            rl->addWidget(cancelBtn);
        }

        // Status badge
        QString stColor = (statut == "APPROVED" || statut == "confirmee") ? "#00b894" :
                           (statut == "CANCELLED" || statut == "annulee") ? "#e17055" : "#fdcb6e";
        QString stBg = (statut == "APPROVED" || statut == "confirmee") ? "#e8f8f5" :
                        (statut == "CANCELLED" || statut == "annulee") ? "#fef3e2" : "#fef9e7";
        QString stText = (statut == "APPROVED" || statut == "confirmee") ? QString::fromUtf8("Confirmed") :
                          (statut == "CANCELLED" || statut == "annulee") ? QString::fromUtf8("Cancelled") :
                          QString::fromUtf8("Pending");
        QLabel *stBadge = new QLabel(stText, row);
        stBadge->setStyleSheet(QString("QLabel{font-size:9px;font-weight:bold;color:%1;"
            "background:%2;border-radius:8px;padding:4px 10px;border:none;}").arg(stColor, stBg));
        rl->addWidget(stBadge);

        m_reservationsLayout->addWidget(row);

        // Separator
        QFrame *sep = new QFrame(this);
        sep->setFrameShape(QFrame::HLine);
        sep->setStyleSheet("QFrame{background:rgba(0,0,0,0.04);max-height:1px;border:none;}");
        m_reservationsLayout->addWidget(sep);
    }
}

void ParkingWidget::onVehicleSelected(int vehiculeId)
{
    qDebug() << "[PARKING] onVehicleSelected called with id:" << vehiculeId;
    QVariantMap veh = ParkingDBManager::instance().getVehicule(vehiculeId);
    qDebug() << "[PARKING] getVehicule returned" << veh.size() << "keys:" << veh.keys();
    if(veh.isEmpty()){
        QMessageBox::warning(this, "",
            QString::fromUtf8("Vehicle not found (id=%1). Check database connection and CARS table.")
            .arg(vehiculeId));
        return;
    }

    m_currentVehiculeId = vehiculeId;
    m_selectedPlate = veh["immatriculation"].toString();
    m_selectedModel = veh["modele"].toString();
    m_selectedTarif = veh.contains("tarif_seance") ? veh["tarif_seance"].toDouble() : 15.0;

    // Update the car label in the home page
    if(m_selectedCarLabel)
        m_selectedCarLabel->setText(
            QString::fromUtf8("\xf0\x9f\x9a\x97 %1 \xe2\x80\x94 %2")
            .arg(m_selectedModel, m_selectedPlate));

    // Configure sensor manager with the vehicle's port
    QString port = veh["port_serie"].toString();
    int baud = veh["baud_rate"].toInt();
    if(baud <= 0) baud = 9600;
    // Attempt connection (will work if physical port exists)
    m_sensorMgr->connectToPort(port, baud);

    // Navigate to maneuver picker
    m_stack->setCurrentIndex(1);
}

// ═══════════════════════════════════════════════════════════════════
//  ANALYTICS PAGE — Smart Coach & Performance Overview
// ═══════════════════════════════════════════════════════════════════

QWidget* ParkingWidget::createAnalyticsPage()
{
    QScrollArea *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setStyleSheet(
        "QScrollArea{border:none;background:#f5f5f5;}"
        "QScrollBar:vertical{width:6px;background:transparent;}"
        "QScrollBar::handle:vertical{background:#ccc;border-radius:3px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    QWidget *content = new QWidget();
    content->setObjectName("analyticsContent");
    content->setStyleSheet("QWidget#analyticsContent{background:#f5f5f5;}");
    QVBoxLayout *lay = new QVBoxLayout(content);
    lay->setContentsMargins(24, 20, 24, 20);
    lay->setSpacing(16);

    // Title
    QLabel *title = new QLabel(QString::fromUtf8("🧠 Smart Coach — Performance Analysis"), content);
    title->setStyleSheet("QLabel{font-size:16px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    lay->addWidget(title);

    // ── Stats summary row ──
    QHBoxLayout *statsRow = new QHBoxLayout();
    statsRow->setSpacing(12);

    auto &db = ParkingDBManager::instance();
    int totalSessions = db.getSessionCount(m_currentEleveId);
    int totalSuccess = db.getSuccessCount(m_currentEleveId);
    double taux = db.getTauxReussite(m_currentEleveId);
    int totalSec = db.getTotalPracticeSeconds(m_currentEleveId);
    double hours = totalSec / 3600.0;

    auto makeCard = [&](const QString &icon, const QString &val, const QString &label, const QString &color) {
        QFrame *card = new QFrame(content);
        static int cid = 0;
        QString oid = QString("anC%1").arg(cid++);
        card->setObjectName(oid);
        card->setStyleSheet(QString("QFrame#%1{background:white;border-radius:12px;"
            "border:1px solid #e8e8e8;}").arg(oid));
        QVBoxLayout *cl = new QVBoxLayout(card);
        cl->setContentsMargins(16,12,16,12); cl->setSpacing(4);
        cl->setAlignment(Qt::AlignCenter);
        QLabel *ic = new QLabel(icon, card);
        ic->setAlignment(Qt::AlignCenter);
        ic->setStyleSheet("QLabel{font-size:24px;background:transparent;border:none;}");
        cl->addWidget(ic);
        QLabel *v = new QLabel(val, card);
        v->setAlignment(Qt::AlignCenter);
        v->setStyleSheet(QString("QLabel{font-size:20px;font-weight:bold;color:%1;"
            "background:transparent;border:none;}").arg(color));
        cl->addWidget(v);
        QLabel *l = new QLabel(label, card);
        l->setAlignment(Qt::AlignCenter);
        l->setStyleSheet("QLabel{font-size:10px;color:#636e72;background:transparent;border:none;}");
        cl->addWidget(l);
        return card;
    };

    statsRow->addWidget(makeCard(QString::fromUtf8("📋"), QString::number(totalSessions), "Sessions", "#0984e3"));
    statsRow->addWidget(makeCard(QString::fromUtf8("✅"), QString::number(totalSuccess), QString::fromUtf8("Passed"), "#00b894"));
    statsRow->addWidget(makeCard(QString::fromUtf8("🏆"), QString("%1%").arg(taux, 0, 'f', 0), "Taux", taux >= 70 ? "#00b894" : "#e17055"));
    statsRow->addWidget(makeCard(QString::fromUtf8("⏱"), QString("%1h").arg(hours, 0, 'f', 1), "Pratique", "#6c5ce7"));
    lay->addLayout(statsRow);

    // ── Maneuver mastery ──
    QFrame *masteryCard = new QFrame(content);
    masteryCard->setObjectName("masteryCard");
    masteryCard->setStyleSheet("QFrame#masteryCard{background:white;border-radius:12px;border:1px solid #e8e8e8;}");
    QVBoxLayout *ml = new QVBoxLayout(masteryCard);
    ml->setContentsMargins(20,16,20,16);
    ml->setSpacing(10);

    QLabel *mTitle = new QLabel(QString::fromUtf8("📊 Mastery by Maneuver"), masteryCard);
    mTitle->setStyleSheet("QLabel{font-size:14px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
    ml->addWidget(mTitle);

    QStringList manNames = {"Parallel", "Perpendicular", "Diagonal",
                            "Reverse", "U-turn"};
    QStringList colors = {"#00b894", "#0984e3", "#6c5ce7", "#e17055", "#fdcb6e"};

    for (int i = 0; i < m_maneuverNames.size() && i < manNames.size(); i++) {
        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing(10);

        QLabel *nm = new QLabel(manNames[i], masteryCard);
        nm->setFixedWidth(120);
        nm->setStyleSheet("QLabel{font-size:11px;color:#2d3436;font-weight:600;background:transparent;border:none;}");
        row->addWidget(nm);

        int attempts = m_maneuverAttemptCounts.value(i, 0);
        int successes = m_maneuverSuccessCounts.value(i, 0);
        int pct = attempts > 0 ? (successes * 100 / attempts) : 0;

        QProgressBar *bar = new QProgressBar(masteryCard);
        bar->setRange(0, 100); bar->setValue(pct);
        bar->setTextVisible(false);
        bar->setFixedHeight(10);
        bar->setStyleSheet(QString(
            "QProgressBar{background:#f0f0f0;border-radius:5px;border:none;}"
            "QProgressBar::chunk{background:%1;border-radius:5px;}").arg(colors[i]));
        row->addWidget(bar, 1);

        QLabel *pctLabel = new QLabel(QString("%1%").arg(pct), masteryCard);
        pctLabel->setFixedWidth(40);
        pctLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        pctLabel->setStyleSheet(QString("QLabel{font-size:11px;font-weight:bold;color:%1;"
            "background:transparent;border:none;}").arg(colors[i]));
        row->addWidget(pctLabel);

        QLabel *detail = new QLabel(QString("%1/%2").arg(successes).arg(attempts), masteryCard);
        detail->setFixedWidth(50);
        detail->setStyleSheet("QLabel{font-size:9px;color:#b2bec3;background:transparent;border:none;}");
        row->addWidget(detail);

        ml->addLayout(row);
    }
    lay->addWidget(masteryCard);

    // ── Exam Pass Probability ──
    double examProb = computeExamPassProbability();
    QFrame *probCard = new QFrame(content);
    probCard->setObjectName("probCard");
    QString probBorder = examProb >= 70 ? "#00b894" : examProb >= 40 ? "#fdcb6e" : "#e17055";
    probCard->setStyleSheet(QString("QFrame#probCard{background:white;border-radius:12px;"
        "border:2px solid %1;}").arg(probBorder));
    QHBoxLayout *probL = new QHBoxLayout(probCard);
    probL->setContentsMargins(20,16,20,16);
    probL->setSpacing(20);

    QLabel *probIcon = new QLabel(QString::fromUtf8("\xf0\x9f\x8e\xaf"), probCard);
    probIcon->setFixedSize(56, 56);
    probIcon->setAlignment(Qt::AlignCenter);
    probIcon->setStyleSheet("QLabel{font-size:32px;background:transparent;border:none;}");
    probL->addWidget(probIcon);

    QVBoxLayout *probTextCol = new QVBoxLayout();
    probTextCol->setSpacing(4);
    QLabel *probTitle = new QLabel(QString::fromUtf8("Exam Pass Probability"), probCard);
    probTitle->setStyleSheet("QLabel{font-size:14px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    probTextCol->addWidget(probTitle);

    QLabel *probValue = new QLabel(QString("%1%").arg(examProb, 0, 'f', 0), probCard);
    probValue->setStyleSheet(QString("QLabel{font-size:36px;font-weight:bold;color:%1;"
        "background:transparent;border:none;}").arg(probBorder));
    probTextCol->addWidget(probValue);

    QString probDesc;
    if (examProb >= 80) probDesc = QString::fromUtf8("Excellent! You are ready for the exam.");
    else if (examProb >= 60) probDesc = QString::fromUtf8("Good progress. A few more sessions and you'll be ready.");
    else if (examProb >= 40) probDesc = QString::fromUtf8("Keep practicing. Focus on your weakest maneuvers.");
    else probDesc = QString::fromUtf8("More practice needed. Build up experience on all maneuvers.");
    QLabel *probDescLabel = new QLabel(probDesc, probCard);
    probDescLabel->setStyleSheet("QLabel{font-size:11px;color:#636e72;background:transparent;border:none;}");
    probDescLabel->setWordWrap(true);
    probTextCol->addWidget(probDescLabel);
    probL->addLayout(probTextCol, 1);

    // Progress ring visual (simplified as progress bar)
    QProgressBar *probBar = new QProgressBar(probCard);
    probBar->setRange(0, 100);
    probBar->setValue((int)examProb);
    probBar->setFixedSize(80, 80);
    probBar->setTextVisible(true);
    probBar->setFormat("%p%");
    probBar->setStyleSheet(QString(
        "QProgressBar{background:#f0f2f5;border:none;border-radius:40px;"
        "text-align:center;font-size:14px;font-weight:bold;color:%1;}"
        "QProgressBar::chunk{background:%1;border-radius:40px;}").arg(probBorder));
    probL->addWidget(probBar);

    lay->addWidget(probCard);

    // ── Performance Trend in Analytics ──
    {
        auto sessions2 = ParkingDBManager::instance().getSessionsEleve(m_currentEleveId, 20);
        QList<int> sc2; QList<bool> su2;
        for (int i = sessions2.size() - 1; i >= 0; i--) {
            const auto &s2 = sessions2[i];
            bool ok2 = s2["reussi"].toBool();
            int err2 = s2["nb_erreurs"].toInt();
            int cal2 = s2["nb_calages"].toInt();
            int score2 = 100;
            if (!ok2) score2 -= 30;
            score2 -= err2 * 10; score2 -= cal2 * 15;
            sc2.append(qMax(0, qMin(100, score2)));
            su2.append(ok2);
        }
        if (!sc2.isEmpty()) {
            QFrame *tCard = new QFrame(content);
            tCard->setObjectName("anTrend");
            tCard->setStyleSheet("QFrame#anTrend{background:white;border-radius:12px;"
                "border:1px solid #e8e8e8;}");
            QVBoxLayout *tl = new QVBoxLayout(tCard);
            tl->setContentsMargins(16, 12, 16, 12);
            PerformanceTrendWidget *tw = new PerformanceTrendWidget(tCard);
            tw->setData(sc2, su2);
            tw->setMinimumHeight(200);
            tl->addWidget(tw);
            lay->addWidget(tCard);
        }
    }

    // ── Smart Coach Advice ──
    QFrame *coachCard = new QFrame(content);
    coachCard->setObjectName("coachCard");
    coachCard->setStyleSheet("QFrame#coachCard{background:white;border-radius:12px;border:1px solid #e8e8e8;}");
    QVBoxLayout *cl = new QVBoxLayout(coachCard);
    cl->setContentsMargins(20,16,20,16);
    cl->setSpacing(10);

    QLabel *cTitle = new QLabel(QString::fromUtf8("💡 Tips du Coach"), coachCard);
    cTitle->setStyleSheet("QLabel{font-size:14px;font-weight:bold;color:#2d3436;background:transparent;border:none;}");
    cl->addWidget(cTitle);

    QStringList tips;
    if (taux < 50)
        tips << QString::fromUtf8("📌 Focus on the basics : stop completely at every stop sign.");
    if (taux >= 50 && taux < 80)
        tips << QString::fromUtf8("\xf0\x9f\x93\x8c Good progress! Work on fluidity.");
    if (taux >= 80)
        tips << QString::fromUtf8("🌟 Excellent! You are ready for the exam.");
    if (hours < 2)
        tips << QString::fromUtf8("⏱ Increase your practice time — minimum target 8h.");
    if (totalSessions > 0 && m_nbCalages > 0)
        tips << QString::fromUtf8("🔧 Watch out for stalls — release the clutch gradually.");
    if (totalSessions == 0)
        tips << QString::fromUtf8("🚀 Start your first session to unlock analytics!");

    if (tips.isEmpty())
        tips << QString::fromUtf8("✅ Keep it up, you're on the right track !");

    for (const auto &tip : tips) {
        QLabel *tipLabel = new QLabel(tip, coachCard);
        tipLabel->setWordWrap(true);
        tipLabel->setStyleSheet("QLabel{font-size:12px;color:#2d3436;background:#f8f9fa;"
            "border-radius:8px;padding:10px 14px;border:none;}");
        cl->addWidget(tipLabel);
    }
    lay->addWidget(coachCard);

    // ── Back button ──
    QPushButton *backBtn = new QPushButton(QString::fromUtf8("← Back"), content);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(
        "QPushButton{background:#00b894;color:white;border:none;border-radius:10px;"
        "padding:10px 24px;font-size:12px;font-weight:bold;}"
        "QPushButton:hover{background:#00a884;}");
    connect(backBtn, &QPushButton::clicked, this, [this]() { m_stack->setCurrentIndex(1); });
    lay->addWidget(backBtn, 0, Qt::AlignLeft);

    lay->addStretch();
    scroll->setWidget(content);
    return scroll;
}

// ═══════════════════════════════════════════════════════════════════
//  REFRESH ANALYTICS — Update analytics page data
// ═══════════════════════════════════════════════════════════════════

void ParkingWidget::refreshAnalytics()
{
    // Analytics page is rebuilt each time via createAnalyticsPage()
    // This method allows external refresh if needed
    updateDashboard();
}

// ═══════════════════════════════════════════════════════════════════
//  SESSION REPLAY — View past session details
// ═══════════════════════════════════════════════════════════════════

void ParkingWidget::showSessionReplay(int sessionId)
{
    auto session = ParkingDBManager::instance().getSession(sessionId);
    if (session.isEmpty()) return;

    bool ok = session["reussi"].toInt() == 1;
    int dur = session["duree_secondes"].toInt();
    int errors = session["nb_erreurs"].toInt();
    int calages = session["nb_calages"].toInt();
    QString manType = session["manoeuvre_type"].toString();

    QString msg = QString::fromUtf8(
        "<h3>%1 Session #%2</h3>"
        "<p><b>Maneuver:</b> %3</p>"
        "<p><b>Duration:</b> %4:%5</p>"
        "<p><b>Result:</b> %6</p>"
        "<p><b>Errors:</b> %7 · <b>Stalls:</b> %8</p>")
        .arg(ok ? QString::fromUtf8("✅") : QString::fromUtf8("❌"))
        .arg(sessionId)
        .arg(manType)
        .arg(dur/60).arg(dur%60, 2, 10, QChar('0'))
        .arg(ok ? QString::fromUtf8("Passed") : QString::fromUtf8("Failed"))
        .arg(errors).arg(calages);

    QMessageBox box(this);
    box.setWindowTitle(QString::fromUtf8("Session Detail"));
    box.setTextFormat(Qt::RichText);
    box.setText(msg);
    box.exec();
}

// ═══════════════════════════════════════════════════════════════════
//  VOICE ASSISTANT — QTextToSpeech integration
// ═══════════════════════════════════════════════════════════════════

void ParkingWidget::speak(const QString &text)
{
    if (!m_voiceEnabled || !m_tts) return;
    // Stop any ongoing speech before speaking new text
    if (m_tts->state() == QTextToSpeech::Speaking)
        m_tts->stop();
    m_tts->say(text);
}

void ParkingWidget::speakStep(int stepIndex)
{
    if (!m_voiceEnabled || !m_tts) return;
    if (m_currentManeuver < 0 || m_currentManeuver >= m_allManeuvers.size()) return;
    const auto &steps = m_allManeuvers[m_currentManeuver];
    if (stepIndex < 0 || stepIndex >= steps.size()) return;

    const auto &step = steps[stepIndex];
    // Build voice message: step number + instruction + assistant message
    QString voiceMsg = QString::fromUtf8("Step %1. %2.")
        .arg(stepIndex + 1).arg(step.instruction);
    if (!step.assistMsg.isEmpty())
        voiceMsg += " " + step.assistMsg;

    speak(voiceMsg);
}

void ParkingWidget::speakSensorWarning(const QString &zone, int distCm)
{
    if (!m_voiceEnabled || !m_tts) return;
    // Cooldown: don't spam voice (minimum 3 seconds between sensor warnings)
    if (m_sensorSpeakCooldown && m_sensorSpeakCooldown->isActive()) return;
    if (m_sensorSpeakCooldown) m_sensorSpeakCooldown->start(3000);

    QString msg;
    if (distCm < 10) {
        msg = QString::fromUtf8("Warning! Obstacle very close %1. %2 centimeters.")
            .arg(zone).arg(distCm);
    } else if (distCm < 25) {
        msg = QString::fromUtf8("Warning %1. %2 centimeters.")
            .arg(zone).arg(distCm);
    } else if (distCm < 50) {
        msg = QString::fromUtf8("Obstacle detected %1 at %2 centimeters.")
            .arg(zone).arg(distCm);
    } else {
        return; // No warning needed for distances > 50cm
    }
    speak(msg);
}

// ═══════════════════════════════════════════════════════════════════
//  REAL EXAM MODE — Full ATTT Parking Exam with Sensor Phases
//
//  Flow (based on real ATTT exam protocol):
//  1. RIGHT_OBSTACLE  → "Turn slightly LEFT"
//  2. LEFT_OBSTACLE   → "Turn slightly RIGHT"
//  3. LEFT_SENSOR1    → "FULL LOCK LEFT"
//  4. RIGHT_SENSOR2   → "STOP" → 5s → "FULL LOCK RIGHT"
//  5. REAR_RIGHT      → "STOP" → 3s → "FULL LOCK LEFT"
//  6. BOTH_REAR_OBS   → "STOP"
//  7. BOTH_REAR_FINAL → "STOP + CONGRATULATIONS"
// ═══════════════════════════════════════════════════════════════════

void ParkingWidget::startRealExam()
{
    m_realExamPhase = EXAM_WAITING_START;
    m_realExamErrors = 0;

    // Show the exam instruction card
    if (m_examInstructionCard) {
        m_examInstructionCard->setVisible(true);
        m_examInstructionCard->setStyleSheet("QFrame#examInstCard{background:white;border-radius:16px;"
            "border:2px solid #00b894;}");
    }

    showExamInstruction(
        QString::fromUtf8("\xf0\x9f\x9a\x97"),
        "ATTT Parking Exam",
        "Begin reversing slowly.\nThe sensors will guide you step by step.\n\n"
        "The exam will follow the real ATTT protocol.\n"
        "Listen carefully to each instruction.",
        "#00b894");

    speak("ATTT Parking Exam started. Begin reversing slowly. Follow the sensor instructions.");
}

void ParkingWidget::handleRealExamEvent(const QString &event)
{
    switch (m_realExamPhase) {

    // ── Phase 1: Waiting for right obstacle ──
    case EXAM_WAITING_START:
    case EXAM_RIGHT_OBSTACLE:
        if (event == "RIGHT_OBSTACLE") {
            m_realExamPhase = EXAM_LEFT_OBSTACLE;
            showExamInstruction(
                QString::fromUtf8("\xe2\xac\x85"),
                "Turn slightly LEFT",
                "Right sensor detected an obstacle.\nSteer gently to the LEFT to avoid it.",
                "#0984e3");
            speak("Right obstacle detected. Turn the steering wheel slightly to the left.");
            flashSensorZone(m_zRight, "#e17055");
        }
        break;

    // ── Phase 2: Waiting for left obstacle ──
    case EXAM_LEFT_OBSTACLE:
        if (event == "LEFT_OBSTACLE") {
            m_realExamPhase = EXAM_LEFT_SENSOR1;
            showExamInstruction(
                QString::fromUtf8("\xe2\x9e\xa1"),
                "Turn slightly RIGHT",
                "Left sensor detected an obstacle.\nSteer gently to the RIGHT.",
                "#0984e3");
            speak("Left obstacle detected. Turn the steering wheel slightly to the right.");
            flashSensorZone(m_zLeft, "#e17055");
        }
        break;

    // ── Phase 3: Left sensor detects Sensor 1 → FULL LEFT ──
    case EXAM_LEFT_SENSOR1:
        if (event == "LEFT_SENSOR1") {
            m_realExamPhase = EXAM_RIGHT_SENSOR2_STOP;
            showExamInstruction(
                QString::fromUtf8("\xf0\x9f\x94\x84"),
                "FULL LOCK LEFT",
                "Left sensor aligned with reference point 1.\nTurn the steering wheel ALL the way to the LEFT.\nKeep reversing slowly.",
                "#6c5ce7");
            speak("Left sensor detected reference point 1. Turn the steering wheel fully to the left. Keep reversing.");
            flashSensorZone(m_zLeft, "#6c5ce7");
        }
        break;

    // ── Phase 4: Right sensor detects Sensor 2 → STOP then FULL RIGHT ──
    case EXAM_RIGHT_SENSOR2_STOP:
        if (event == "RIGHT_SENSOR2") {
            m_realExamPhase = EXAM_RIGHT_SENSOR2_TURN;
            showExamInstruction(
                QString::fromUtf8("\xf0\x9f\x9b\x91"),
                "STOP !",
                "Right sensor aligned with reference point 2.\nSTOP the vehicle completely.\n\nWait for the next instruction...",
                "#e17055");
            speak("Stop! Right sensor detected reference point 2. Stop the vehicle. Wait.");
            flashSensorZone(m_zRight, "#e17055");

            // After 5 seconds → turn full right
            m_examDelayTimer->disconnect();
            connect(m_examDelayTimer, &QTimer::timeout, this, [this](){
                m_realExamPhase = EXAM_REAR_RIGHT_STOP;
                showExamInstruction(
                    QString::fromUtf8("\xf0\x9f\x94\x84"),
                    "FULL LOCK RIGHT",
                    "Now turn the steering wheel ALL the way to the RIGHT.\nReverse slowly.",
                    "#e17055");
                speak("Now turn the steering wheel fully to the right. Reverse slowly.");
            });
            m_examDelayTimer->start(5000);
        }
        break;

    // ── Phase 5: Rear right sensor detects sensor → STOP then FULL LEFT ──
    case EXAM_REAR_RIGHT_STOP:
        if (event == "REAR_RIGHT_SENSOR") {
            m_realExamPhase = EXAM_REAR_RIGHT_TURN;
            showExamInstruction(
                QString::fromUtf8("\xf0\x9f\x9b\x91"),
                "STOP !",
                "Rear right sensor detected a reference point.\nSTOP the vehicle.\n\nWait for the next instruction...",
                "#e17055");
            speak("Stop! Rear sensor detected. Stop the vehicle and wait.");
            flashSensorZone(m_zRearR, "#e17055");

            // After 3 seconds → turn full left
            m_examDelayTimer->disconnect();
            connect(m_examDelayTimer, &QTimer::timeout, this, [this](){
                m_realExamPhase = EXAM_BOTH_REAR_OBSTACLE;
                showExamInstruction(
                    QString::fromUtf8("\xf0\x9f\x94\x84"),
                    "FULL LOCK LEFT",
                    "Now turn the steering wheel ALL the way to the LEFT.\nReverse slowly and carefully.",
                    "#6c5ce7");
                speak("Now turn the steering wheel fully to the left. Reverse slowly and carefully.");
            });
            m_examDelayTimer->start(3000);
        }
        break;

    // ── Phase 6: Both rear sensors detect obstacle → STOP ──
    case EXAM_BOTH_REAR_OBSTACLE:
        if (event == "BOTH_REAR_OBSTACLE") {
            m_realExamPhase = EXAM_BOTH_REAR_FINAL;
            showExamInstruction(
                QString::fromUtf8("\xf0\x9f\x9b\x91"),
                "STOP !",
                "Both rear sensors detected an obstacle.\nSTOP immediately!\n\nAlmost done... keep going.",
                "#e17055");
            speak("Stop! Both rear sensors detected an obstacle. Stop the vehicle.");
            flashSensorZone(m_zRearL, "#e17055");
            flashSensorZone(m_zRearR, "#e17055");
        }
        break;

    // ── Phase 7: Both rear sensors detect final sensor → SUCCESS ──
    case EXAM_BOTH_REAR_FINAL:
        if (event == "BOTH_REAR_SENSOR") {
            m_realExamPhase = EXAM_PASSED;
            showExamInstruction(
                QString::fromUtf8("\xf0\x9f\x8f\x86"),
                "CONGRATULATIONS !",
                "Both rear sensors confirmed final position.\n\n"
                "You have successfully completed the parking maneuver!\n"
                "Apply the handbrake, shift to neutral, turn off indicator.",
                "#00b894");

            if (m_examInstructionCard) {
                m_examInstructionCard->setStyleSheet("QFrame#examInstCard{background:#e8f8f5;"
                    "border-radius:16px;border:3px solid #00b894;}");
            }

            speak("Congratulations! You have successfully completed the ATTT parking exam! "
                  "Apply the handbrake, shift to neutral, and turn off the indicator.");

            // Auto-end session after 5 seconds
            QTimer::singleShot(5000, this, [this](){
                if (m_sessionActive) {
                    m_contactObstacle = false;
                    endSession();
                }
            });
        }
        break;

    // Intermediate phases (waiting for timer)
    case EXAM_RIGHT_SENSOR2_TURN:
    case EXAM_REAR_RIGHT_TURN:
        // These are handled by the timer callbacks above
        break;

    case EXAM_PASSED:
    case EXAM_FAILED:
    case EXAM_IDLE:
        break;
    }

    // Check for errors during exam (wrong sensor at wrong time)
    if (m_realExamPhase != EXAM_PASSED && m_realExamPhase != EXAM_FAILED) {
        if (event == "BOTH_REAR_OBSTACLE" && m_realExamPhase < EXAM_BOTH_REAR_OBSTACLE) {
            // Hit obstacle too early
            m_realExamErrors++;
            if (m_realExamErrors >= 2) {
                m_realExamPhase = EXAM_FAILED;
                showExamInstruction(
                    QString::fromUtf8("\xe2\x9d\x8c"),
                    "EXAM FAILED",
                    "Too many errors during the maneuver.\nPlease try again.",
                    "#d63031");
                if (m_examInstructionCard) {
                    m_examInstructionCard->setStyleSheet("QFrame#examInstCard{background:#fde8e8;"
                        "border-radius:16px;border:3px solid #d63031;}");
                }
                speak("Exam failed. Too many errors. Please try again.");
                QTimer::singleShot(3000, this, [this](){
                    if (m_sessionActive) endSession();
                });
            }
        }
    }
}

void ParkingWidget::showExamInstruction(const QString &icon, const QString &title,
                                         const QString &instruction, const QString &color)
{
    if (!m_examInstructionCard || !m_examBigIcon || !m_examBigTitle || !m_examBigInstruction)
        return;

    m_examInstructionCard->setVisible(true);
    m_examBigIcon->setText(icon);
    m_examBigTitle->setText(title);
    m_examBigTitle->setStyleSheet(QString("QLabel{font-size:22px;font-weight:bold;color:%1;"
        "background:transparent;border:none;}").arg(color));
    m_examBigInstruction->setText(instruction);

    // Also update step guide area
    if (m_guidanceMsg) {
        m_guidanceMsg->setText(QString("%1  %2").arg(icon, title));
        m_guidanceMsg->setVisible(true);
    }
}

void ParkingWidget::advanceExamPhase(RealExamPhase nextPhase)
{
    m_realExamPhase = nextPhase;
}

// ═══════════════════════════════════════════════════════════════════
//  WEATHER WIDGET — OpenWeatherMap integration for driving conditions
// ═══════════════════════════════════════════════════════════════════

WeatherWidget::WeatherWidget(QWidget *parent) : QFrame(parent)
{
    setObjectName("weatherCard");
    setStyleSheet("QFrame#weatherCard{background:white;border-radius:16px;"
        "border:1px solid #e2e8f0;}");
    setMinimumHeight(120);

    m_nam = new QNetworkAccessManager(this);
    connect(m_nam, &QNetworkAccessManager::finished, this, &WeatherWidget::onWeatherReply);

    QHBoxLayout *mainL = new QHBoxLayout(this);
    mainL->setContentsMargins(20, 14, 20, 14);
    mainL->setSpacing(16);

    // Left: weather icon + temp
    QVBoxLayout *leftCol = new QVBoxLayout();
    leftCol->setSpacing(4);

    m_iconLabel = new QLabel(QString::fromUtf8("\xe2\x9b\x85"), this);
    m_iconLabel->setFixedSize(48, 48);
    m_iconLabel->setAlignment(Qt::AlignCenter);
    m_iconLabel->setStyleSheet("QLabel{font-size:32px;background:#e8f8f5;border-radius:14px;border:none;}");
    leftCol->addWidget(m_iconLabel);

    m_tempLabel = new QLabel(QString::fromUtf8("--\xc2\xb0") + "C", this);
    m_tempLabel->setAlignment(Qt::AlignCenter);
    m_tempLabel->setStyleSheet("QLabel{font-size:18px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    leftCol->addWidget(m_tempLabel);
    mainL->addLayout(leftCol);

    // Center: condition + wind
    QVBoxLayout *centerCol = new QVBoxLayout();
    centerCol->setSpacing(4);

    QLabel *title = new QLabel(QString::fromUtf8("\xf0\x9f\x8c\xa4  Driving Conditions"), this);
    title->setStyleSheet("QLabel{font-size:12px;font-weight:bold;color:#00b894;"
        "background:transparent;border:none;}");
    centerCol->addWidget(title);

    m_condLabel = new QLabel(QString::fromUtf8("Loading weather..."), this);
    m_condLabel->setStyleSheet("QLabel{font-size:13px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    centerCol->addWidget(m_condLabel);

    m_windLabel = new QLabel("", this);
    m_windLabel->setStyleSheet("QLabel{font-size:10px;color:#636e72;"
        "background:transparent;border:none;}");
    centerCol->addWidget(m_windLabel);
    mainL->addLayout(centerCol, 1);

    // Right: driving tip
    m_tipLabel = new QLabel("", this);
    m_tipLabel->setWordWrap(true);
    m_tipLabel->setFixedWidth(220);
    m_tipLabel->setStyleSheet("QLabel{font-size:10px;color:#00b894;background:#f0faf7;"
        "border-radius:10px;padding:10px 14px;border:none;}");
    mainL->addWidget(m_tipLabel);
}

void WeatherWidget::fetchWeather(const QString &city)
{
    // wttr.in — free weather API, no key required
    QString url = QString("https://wttr.in/%1?format=j1").arg(city);
    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("User-Agent", "WinoApp/1.0");
    m_nam->get(req);
}

void WeatherWidget::onWeatherReply(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError) {
        // Fallback: show default Tunisian weather estimation
        updateUI(QString::fromUtf8("\xe2\x98\x80\xef\xb8\x8f"), "Sunny", 28.0, 55, 12.0);
        reply->deleteLater();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject root = doc.object();

    QString condition = "Clear";
    QString icon = QString::fromUtf8("\xe2\x98\x80\xef\xb8\x8f");
    double temp = 25.0;
    int humidity = 50;
    double windSpeed = 10.0;

    // Parse wttr.in JSON format
    if (root.contains("current_condition")) {
        QJsonArray ccArr = root["current_condition"].toArray();
        if (!ccArr.isEmpty()) {
            QJsonObject cc = ccArr.first().toObject();
            temp = cc["temp_C"].toString().toDouble();
            humidity = cc["humidity"].toString().toInt();
            windSpeed = cc["windspeedKmph"].toString().toDouble();

            // Get weather description
            QJsonArray descArr = cc["weatherDesc"].toArray();
            if (!descArr.isEmpty())
                condition = descArr.first().toObject()["value"].toString();

            // Map condition to icon
            QString condLower = condition.toLower();
            if (condLower.contains("rain") || condLower.contains("drizzle"))
                icon = QString::fromUtf8("\xf0\x9f\x8c\xa7");
            else if (condLower.contains("cloud") || condLower.contains("overcast"))
                icon = QString::fromUtf8("\xe2\x9b\x85");
            else if (condLower.contains("storm") || condLower.contains("thunder"))
                icon = QString::fromUtf8("\xe2\x9b\x88");
            else if (condLower.contains("snow"))
                icon = QString::fromUtf8("\xe2\x9d\x84");
            else if (condLower.contains("fog") || condLower.contains("mist") || condLower.contains("haze"))
                icon = QString::fromUtf8("\xf0\x9f\x8c\xab");
            else if (condLower.contains("sunny") || condLower.contains("clear"))
                icon = QString::fromUtf8("\xe2\x98\x80\xef\xb8\x8f");
            else if (condLower.contains("partly"))
                icon = QString::fromUtf8("\xe2\x9b\x85");
            else
                icon = QString::fromUtf8("\xe2\x98\x80\xef\xb8\x8f");
        }
    }

    updateUI(icon, condition, temp, humidity, windSpeed);
    emit weatherLoaded(condition, temp, humidity, windSpeed);
    reply->deleteLater();
}

void WeatherWidget::updateUI(const QString &icon, const QString &condition,
                              double temp, int humidity, double windSpeed)
{
    m_iconLabel->setText(icon);
    m_tempLabel->setText(QString("%1").arg(temp, 0, 'f', 1) + QString::fromUtf8("\xc2\xb0") + "C");
    m_condLabel->setText(condition);
    m_windLabel->setText(QString::fromUtf8(
        "\xf0\x9f\x92\xa8 Wind: %1 km/h  \xc2\xb7  \xf0\x9f\x92\xa7 Humidity: %2%")
        .arg(windSpeed, 0, 'f', 0).arg(humidity));
    m_tipLabel->setText(drivingTip(condition, windSpeed, humidity));

    // Color the card border based on conditions
    QString borderColor = "#00b894"; // good
    if (condition.contains("Rain") || condition.contains("Storm") || windSpeed > 50)
        borderColor = "#e17055"; // bad
    else if (condition.contains("Cloud") || condition.contains("Fog") || windSpeed > 30)
        borderColor = "#fdcb6e"; // moderate
    setStyleSheet(QString("QFrame#weatherCard{background:white;border-radius:16px;"
        "border:2px solid %1;}").arg(borderColor));
}

QString WeatherWidget::drivingTip(const QString &condition, double windSpeed, int humidity)
{
    QString cond = condition.toLower();
    if (cond.contains("rain") || cond.contains("drizzle") || cond.contains("shower"))
        return QString::fromUtf8(
            "\xf0\x9f\x8c\xa7 Wet road! Increase safety distances by x2. "
            "Be extra careful when reversing \xe2\x80\x94 tires have less grip.");
    if (cond.contains("storm") || cond.contains("thunder"))
        return QString::fromUtf8(
            "\xe2\x9b\x88 Stormy conditions. Consider postponing practice. "
            "If you must drive, go very slowly and keep headlights on.");
    if (cond.contains("fog") || cond.contains("mist") || cond.contains("haze"))
        return QString::fromUtf8(
            "\xf0\x9f\x8c\xab Reduced visibility! Use fog lights. "
            "Sensors are more important than ever \xe2\x80\x94 trust the beeps.");
    if (windSpeed > 40)
        return QString::fromUtf8(
            "\xf0\x9f\x92\xa8 Strong wind! Be careful with steering corrections. "
            "Wind gusts can push the car during maneuvers.");
    if (cond.contains("snow") || cond.contains("ice") || cond.contains("sleet"))
        return QString::fromUtf8(
            "\xe2\x9d\x84 Icy conditions. Very gentle movements on steering and brakes. "
            "Allow triple the normal safety distance.");
    if (humidity > 85)
        return QString::fromUtf8(
            "\xf0\x9f\x92\xa7 High humidity \xe2\x80\x94 windows may fog up. "
            "Keep ventilation on. Check visibility before each maneuver.");
    return QString::fromUtf8(
        "\xe2\x98\x80\xef\xb8\x8f Great conditions for practice! "
        "Perfect weather to work on your weakest maneuver.");
}

// ═══════════════════════════════════════════════════════════════════
//  PERFORMANCE TREND WIDGET — Line chart with QPainter
// ═══════════════════════════════════════════════════════════════════

PerformanceTrendWidget::PerformanceTrendWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumHeight(180);
    setStyleSheet("background:transparent;");
}

void PerformanceTrendWidget::setData(const QList<int> &scores, const QList<bool> &success)
{
    m_scores = scores;
    m_success = success;
    update();
}

void PerformanceTrendWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    double W = width(), H = height();
    double pad = 40, top = 20, bot = 30;
    double chartW = W - 2*pad, chartH = H - top - bot;

    // Background
    QLinearGradient bg(0,0,0,H);
    bg.setColorAt(0, QColor(248,250,252));
    bg.setColorAt(1, QColor(241,243,247));
    p.setBrush(bg);
    p.setPen(QPen(QColor(226,232,240), 1));
    p.drawRoundedRect(QRectF(0,0,W,H), 12, 12);

    if (m_scores.isEmpty()) {
        p.setPen(QColor(178,190,195));
        p.setFont(QFont("Segoe UI", 11));
        p.drawText(rect(), Qt::AlignCenter,
            QString::fromUtf8("No session data yet\nStart practicing to see your trend!"));
        return;
    }

    // Title
    p.setFont(QFont("Segoe UI", 10, QFont::Bold));
    p.setPen(QColor(45,52,54));
    p.drawText(QRectF(pad, 4, chartW, 16), Qt::AlignLeft,
        QString::fromUtf8("\xf0\x9f\x93\x88 Score Trend (last %1 sessions)").arg(m_scores.size()));

    // Grid lines
    p.setPen(QPen(QColor(226,232,240), 1, Qt::DashLine));
    for (int y = 0; y <= 100; y += 25) {
        double py = top + chartH * (1.0 - y / 100.0);
        p.drawLine(QPointF(pad, py), QPointF(pad + chartW, py));
        p.setFont(QFont("Segoe UI", 7));
        p.setPen(QColor(178,190,195));
        p.drawText(QRectF(2, py - 7, pad - 6, 14), Qt::AlignRight | Qt::AlignVCenter,
            QString::number(y));
        p.setPen(QPen(QColor(226,232,240), 1, Qt::DashLine));
    }

    // Pass threshold line (50%)
    double passY = top + chartH * 0.5;
    p.setPen(QPen(QColor(0,184,148, 80), 1.5, Qt::DashLine));
    p.drawLine(QPointF(pad, passY), QPointF(pad + chartW, passY));
    p.setFont(QFont("Segoe UI", 7, QFont::Bold));
    p.setPen(QColor(0,184,148,120));
    p.drawText(QPointF(pad + chartW - 50, passY - 4), "PASS");

    int n = m_scores.size();
    double stepX = n > 1 ? chartW / (n - 1) : chartW;

    // Area fill under the curve
    if (n > 1) {
        QPainterPath area;
        area.moveTo(pad, top + chartH);
        for (int i = 0; i < n; i++) {
            double x = pad + i * stepX;
            double y = top + chartH * (1.0 - m_scores[i] / 100.0);
            if (i == 0) area.lineTo(x, y);
            else {
                double prevX = pad + (i-1) * stepX;
                double prevY = top + chartH * (1.0 - m_scores[i-1] / 100.0);
                area.cubicTo(prevX + stepX*0.4, prevY, x - stepX*0.4, y, x, y);
            }
        }
        area.lineTo(pad + (n-1) * stepX, top + chartH);
        area.closeSubpath();

        QLinearGradient fill(0, top, 0, top + chartH);
        fill.setColorAt(0, QColor(0, 184, 148, 40));
        fill.setColorAt(1, QColor(0, 184, 148, 5));
        p.setBrush(fill);
        p.setPen(Qt::NoPen);
        p.drawPath(area);
    }

    // Line
    if (n > 1) {
        QPainterPath line;
        for (int i = 0; i < n; i++) {
            double x = pad + i * stepX;
            double y = top + chartH * (1.0 - m_scores[i] / 100.0);
            if (i == 0) line.moveTo(x, y);
            else {
                double prevX = pad + (i-1) * stepX;
                double prevY = top + chartH * (1.0 - m_scores[i-1] / 100.0);
                line.cubicTo(prevX + stepX*0.4, prevY, x - stepX*0.4, y, x, y);
            }
        }
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(0, 184, 148), 2.5));
        p.drawPath(line);
    }

    // Data points
    for (int i = 0; i < n; i++) {
        double x = pad + i * stepX;
        double y = top + chartH * (1.0 - m_scores[i] / 100.0);
        bool ok = i < m_success.size() && m_success[i];
        QColor dotColor = ok ? QColor(0, 184, 148) : QColor(225, 112, 85);

        // Outer ring
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(255, 255, 255));
        p.drawEllipse(QPointF(x, y), 6, 6);
        p.setBrush(dotColor);
        p.drawEllipse(QPointF(x, y), 4, 4);

        // Score label on last point
        if (i == n - 1) {
            p.setFont(QFont("Segoe UI", 9, QFont::Bold));
            p.setPen(dotColor);
            p.drawText(QRectF(x - 20, y - 20, 40, 14), Qt::AlignCenter,
                QString::number(m_scores[i]));
        }
    }

    // Trend arrow
    if (n >= 3) {
        int last3Avg = 0, prev3Avg = 0;
        int cnt1 = 0, cnt2 = 0;
        for (int i = n-1; i >= qMax(0, n-3); i--) { last3Avg += m_scores[i]; cnt1++; }
        for (int i = qMax(0, n-4); i >= qMax(0, n-6); i--) { prev3Avg += m_scores[i]; cnt2++; }
        if (cnt1 > 0) last3Avg /= cnt1;
        if (cnt2 > 0) prev3Avg /= cnt2;

        QString trend;
        QColor trendCol;
        if (last3Avg > prev3Avg + 5) {
            trend = QString::fromUtf8("\xe2\xac\x86 Improving");
            trendCol = QColor(0, 184, 148);
        } else if (last3Avg < prev3Avg - 5) {
            trend = QString::fromUtf8("\xe2\xac\x87 Declining");
            trendCol = QColor(225, 112, 85);
        } else {
            trend = QString::fromUtf8("\xe2\x86\x94 Stable");
            trendCol = QColor(253, 203, 110);
        }
        p.setFont(QFont("Segoe UI", 9, QFont::Bold));
        p.setPen(trendCol);
        p.drawText(QRectF(pad, H - bot + 6, chartW, 18), Qt::AlignRight, trend);
    }
}

// ═══════════════════════════════════════════════════════════════════
//  VIDEO TUTORIALS — YouTube links per maneuver
// ═══════════════════════════════════════════════════════════════════

void ParkingWidget::initVideoTutorials()
{
    // URLs par défaut (fallback si pas de lien dans la DB)
    m_videoUrls = {
        "https://www.youtube.com/results?search_query=cr%C3%A9neau+parking+permis+tunisie",
        "https://www.youtube.com/results?search_query=bataille+parking+permis+tunisie",
        "https://www.youtube.com/results?search_query=%C3%A9pi+parking+permis+tunisie",
        "https://www.youtube.com/results?search_query=marche+arriere+permis+tunisie",
        "https://www.youtube.com/results?search_query=demi+tour+permis+tunisie"
    };

    // Clés dans le même ordre que m_maneuverNames
    static const QStringList keys = {
        "creneau", "bataille", "epi", "marche_arriere", "demi_tour"
    };

    if (!ParkingDBManager::instance().isConnected()) return;

    // Charge les liens depuis PARKING_MANEUVER_STEPS (colonne youtube_url)
    auto rows = ParkingDBManager::instance().getAllManeuverVideoUrls();
    QMap<QString,QString> urlMap;
    for (const auto &r : rows)
        urlMap[r["maneuver_type"].toString()] = r["youtube_url"].toString();

    for (int i = 0; i < keys.size(); i++) {
        QString url = urlMap.value(keys[i], "").trimmed();
        if (!url.isEmpty())
            m_videoUrls[i] = url;
    }
}

QWidget* ParkingWidget::createVideoTutorialCard(int maneuverIndex)
{
    QFrame *card = new QFrame(this);
    QString cid = QString("vidCard%1").arg(maneuverIndex);
    card->setObjectName(cid);
    card->setStyleSheet(QString(
        "QFrame#%1{background:white;border-radius:14px;border:1px solid #e2e8f0;}").arg(cid));

    QHBoxLayout *l = new QHBoxLayout(card);
    l->setContentsMargins(16, 12, 16, 12);
    l->setSpacing(12);

    QLabel *icon = new QLabel(QString::fromUtf8("\xf0\x9f\x8e\xac"), card);
    icon->setFixedSize(36, 36);
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet(QString("QLabel{font-size:18px;background:%1;border-radius:10px;border:none;}")
        .arg(m_maneuverColors.value(maneuverIndex, "#00b894")));
    l->addWidget(icon);

    QVBoxLayout *textCol = new QVBoxLayout();
    textCol->setSpacing(2);
    QLabel *title = new QLabel(
        QString::fromUtf8("\xf0\x9f\x93\xba Video Tutorial: %1")
        .arg(m_maneuverNames.value(maneuverIndex, "Maneuver")), card);
    title->setStyleSheet("QLabel{font-size:12px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    textCol->addWidget(title);

    QLabel *desc = new QLabel(
        QString::fromUtf8("Watch real ATTT exam demonstrations on YouTube"), card);
    desc->setStyleSheet("QLabel{font-size:10px;color:#636e72;background:transparent;border:none;}");
    textCol->addWidget(desc);
    l->addLayout(textCol, 1);

    QPushButton *watchBtn = new QPushButton(QString::fromUtf8("\xe2\x96\xb6  Watch"), card);
    watchBtn->setCursor(Qt::PointingHandCursor);
    watchBtn->setFixedHeight(32);
    watchBtn->setStyleSheet(
        "QPushButton{background:#ff0000;color:white;border:none;border-radius:10px;"
        "padding:0 18px;font-size:11px;font-weight:bold;}"
        "QPushButton:hover{background:#cc0000;}");
    QString url = m_videoUrls.value(maneuverIndex, "");
    connect(watchBtn, &QPushButton::clicked, this, [url](){
        QDesktopServices::openUrl(QUrl(url));
    });
    l->addWidget(watchBtn);

    return card;
}

// ═══════════════════════════════════════════════════════════════════
//  SMART RECOMMENDATION — AI-powered next maneuver suggestion
// ═══════════════════════════════════════════════════════════════════

int ParkingWidget::computeRecommendedManeuver()
{
    // Find the maneuver with lowest mastery that has been attempted,
    // or the first unattempted maneuver
    int worstIdx = 0;
    int worstScore = 101;
    bool hasUnattempted = false;

    for (int i = 0; i < 5; i++) {
        if (m_maneuverAttemptCounts[i] == 0) {
            if (!hasUnattempted) {
                worstIdx = i;
                hasUnattempted = true;
            }
        } else if (!hasUnattempted) {
            int pct = m_maneuverSuccessCounts[i] * 100 / m_maneuverAttemptCounts[i];
            if (pct < worstScore) {
                worstScore = pct;
                worstIdx = i;
            }
        }
    }
    return worstIdx;
}

double ParkingWidget::computeExamPassProbability()
{
    if (m_totalSessionsCount == 0) {
        qDebug() << "[SmartCoach] computeExamPassProbability: m_totalSessionsCount is 0, returning 0.0";
        return 0.0;
    }

    double prob = 0.0;

    // Factor 1: Overall success rate (40% weight)
    double successRate = m_totalSessionsCount > 0 ?
        (double)m_totalSuccessCount / m_totalSessionsCount : 0.0;
    prob += successRate * 40.0;
    qDebug() << "[SmartCoach] m_totalSessionsCount:" << m_totalSessionsCount 
             << "m_totalSuccessCount:" << m_totalSuccessCount 
             << "successRate score:" << (successRate * 40.0);

    // Factor 2: All maneuvers attempted (20% weight)
    int maneuversAttempted = 0;
    int maneuversMastered = 0; // >60% success
    for (int i = 0; i < 5; i++) {
        if (m_maneuverAttemptCounts[i] > 0) maneuversAttempted++;
        if (m_maneuverAttemptCounts[i] > 0 &&
            m_maneuverSuccessCounts[i] * 100 / m_maneuverAttemptCounts[i] >= 60)
            maneuversMastered++;
    }
    prob += (maneuversAttempted / 5.0) * 10.0;
    prob += (maneuversMastered / 5.0) * 10.0;
    qDebug() << "[SmartCoach] maneuversAttempted:" << maneuversAttempted 
             << "maneuversMastered:" << maneuversMastered 
             << "maneuver score:" << ((maneuversAttempted / 5.0) * 10.0 + (maneuversMastered / 5.0) * 10.0);

    // Factor 3: Practice volume (20% weight)
    double volumeScore = qMin(1.0, m_totalSessionsCount / 30.0);
    prob += volumeScore * 20.0;
    qDebug() << "[SmartCoach] volumeScore:" << volumeScore 
             << "volume points:" << (volumeScore * 20.0);

    // Factor 4: Recent performance (last 5 sessions) (20% weight)
    auto sessions = ParkingDBManager::instance().getSessionsEleve(m_currentEleveId, 5);
    int recentSuccess = 0;
    if (!sessions.isEmpty()) {
        for (const auto &s : sessions) {
            if (s["reussi"].toBool()) recentSuccess++;
        }
        prob += (recentSuccess / (double)sessions.size()) * 20.0;
    }
    qDebug() << "[SmartCoach] recent sessions:" << sessions.size() 
             << "recentSuccess:" << recentSuccess 
             << "recent score:" << (sessions.isEmpty() ? 0 : (recentSuccess / (double)sessions.size()) * 20.0);

    qDebug() << "[SmartCoach] computeExamPassProbability Final Prob:" << prob;
    return qMin(99.0, qMax(0.0, prob));
}

QWidget* ParkingWidget::createSmartRecommendationCard()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("smartRecCard");
    card->setStyleSheet("QFrame#smartRecCard{background:white;border-radius:16px;"
        "border:2px solid #6c5ce7;}");

    QVBoxLayout *l = new QVBoxLayout(card);
    l->setContentsMargins(20, 16, 20, 16);
    l->setSpacing(12);

    // Header
    QHBoxLayout *hdr = new QHBoxLayout();
    QLabel *title = new QLabel(QString::fromUtf8(
        "\xf0\x9f\xa7\xa0  Smart Coach Recommendation"), card);
    title->setStyleSheet("QLabel{font-size:14px;font-weight:bold;color:#6c5ce7;"
        "background:transparent;border:none;}");
    hdr->addWidget(title);
    hdr->addStretch();

    // Exam probability badge
    double prob = computeExamPassProbability();
    QString probColor = prob >= 70 ? "#00b894" : prob >= 40 ? "#fdcb6e" : "#e17055";
    QLabel *probBadge = new QLabel(
        QString::fromUtf8("\xf0\x9f\x8e\xaf Exam: %1%").arg(prob, 0, 'f', 0), card);
    probBadge->setStyleSheet(QString(
        "QLabel{font-size:11px;font-weight:bold;color:%1;"
        "background:%2;border-radius:10px;padding:4px 12px;border:none;}")
        .arg(probColor, prob >= 70 ? "#e8f8f5" : prob >= 40 ? "#fef9e7" : "#fef3e2"));
    hdr->addWidget(probBadge);
    l->addLayout(hdr);

    // Recommendation
    m_recommendedManeuver = computeRecommendedManeuver();
    QString recName = m_maneuverNames.value(m_recommendedManeuver, "Parallel");
    QString recIcon = m_maneuverIcons.value(m_recommendedManeuver, "");
    QString recColor = m_maneuverColors.value(m_recommendedManeuver, "#00b894");

    m_recommendLabel = new QLabel("", card);
    m_recommendLabel->setWordWrap(true);
    m_recommendLabel->setStyleSheet("QLabel{font-size:12px;color:#2d3436;"
        "background:#f8f8f8;border-radius:12px;padding:14px 18px;border:none;line-height:1.5;}");

    // Build recommendation text
    QString recText;
    if (m_totalSessionsCount == 0) {
        recText = QString::fromUtf8(
            "\xf0\x9f\x9a\x80 <b>Start with %1 %2</b><br>"
            "This is the most common maneuver on the ATTT exam. "
            "Master it first to build confidence!").arg(recIcon, recName);
    } else if (m_maneuverAttemptCounts[m_recommendedManeuver] == 0) {
        recText = QString::fromUtf8(
            "\xf0\x9f\x86\x95 <b>Try %1 %2</b><br>"
            "You haven't practiced this maneuver yet. "
            "All 5 maneuvers must be mastered for the exam!").arg(recIcon, recName);
    } else {
        int pct = m_maneuverSuccessCounts[m_recommendedManeuver] * 100 /
            m_maneuverAttemptCounts[m_recommendedManeuver];
        recText = QString::fromUtf8(
            "\xf0\x9f\x93\x89 <b>Focus on %1 %2</b> (%3% success)<br>"
            "This is your weakest maneuver. "
            "Practice 3-5 more sessions to improve your score.")
            .arg(recIcon, recName).arg(pct);
    }
    m_recommendLabel->setTextFormat(Qt::RichText);
    m_recommendLabel->setText(recText);
    l->addWidget(m_recommendLabel);

    // Action button
    m_recommendBtn = new QPushButton(
        QString::fromUtf8("%1  Practice %2  \xe2\x86\x92").arg(recIcon, recName), card);
    m_recommendBtn->setCursor(Qt::PointingHandCursor);
    m_recommendBtn->setFixedHeight(42);
    m_recommendBtn->setStyleSheet(QString(
        "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "stop:0 #6c5ce7,stop:1 #a29bfe);color:white;border:none;border-radius:12px;"
        "font-size:13px;font-weight:bold;letter-spacing:0.5px;}"
        "QPushButton:hover{background:#5a4bd1;}"));
    connect(m_recommendBtn, &QPushButton::clicked, this,
        [this](){ openManeuver(m_recommendedManeuver); });
    l->addWidget(m_recommendBtn);

    return card;
}

// ═══════════════════════════════════════════════════════════════════
//  WEATHER CARD — Wrapper for home page
// ═══════════════════════════════════════════════════════════════════

QWidget* ParkingWidget::createWeatherCard()
{
    m_weatherWidget = new WeatherWidget(this);
    auto *shadow = new QGraphicsDropShadowEffect(m_weatherWidget);
    shadow->setBlurRadius(16); shadow->setColor(QColor(0,0,0,15)); shadow->setOffset(0,4);
    m_weatherWidget->setGraphicsEffect(shadow);
    m_weatherWidget->fetchWeather("Tunis");
    return m_weatherWidget;
}

// ═══════════════════════════════════════════════════════════════════
//  PERFORMANCE TREND CARD — Wrapper for analytics
// ═══════════════════════════════════════════════════════════════════

QWidget* ParkingWidget::createPerformanceTrendCard()
{
    QFrame *card = new QFrame(this);
    card->setObjectName("trendCard");
    card->setStyleSheet("QFrame#trendCard{background:white;border-radius:16px;"
        "border:1px solid #e2e8f0;}");
    auto *s = new QGraphicsDropShadowEffect(card);
    s->setBlurRadius(16); s->setColor(QColor(0,0,0,15)); s->setOffset(0,4);
    card->setGraphicsEffect(s);

    QVBoxLayout *l = new QVBoxLayout(card);
    l->setContentsMargins(18, 14, 18, 14);
    l->setSpacing(8);

    m_trendWidget = new PerformanceTrendWidget(card);

    // Load session data for trend
    auto sessions = ParkingDBManager::instance().getSessionsEleve(m_currentEleveId, 20);
    QList<int> scores;
    QList<bool> successes;
    // Reverse to show chronological order
    for (int i = sessions.size() - 1; i >= 0; i--) {
        const auto &s = sessions[i];
        bool ok = s["reussi"].toBool();
        int err = s["nb_erreurs"].toInt();
        int cal = s["nb_calages"].toInt();
        // Compute approximate score
        int score = 100;
        if (!ok) score -= 30;
        score -= err * 10;
        score -= cal * 15;
        score = qMax(0, qMin(100, score));
        scores.append(score);
        successes.append(ok);
    }
    m_trendWidget->setData(scores, successes);

    l->addWidget(m_trendWidget);
    return card;
}

// ═══════════════════════════════════════════════════════════════
// DB SENSOR MESSAGES — load PARKING_MANEUVER_STEPS guidance_message
// for the current maneuver and map them by Arduino event name
// ═══════════════════════════════════════════════════════════════
void ParkingWidget::loadDbSensorMessages(const QString &maneuverType)
{
    m_dbSensorMessages.clear();
    m_eventToDbStep.clear();
    m_dbStopRequired.clear();

    // DB sensor_condition → Arduino event name
    static const QMap<QString,QString> condToEvent = {
        {"capteur_droit=obstacle",       "RIGHT_OBSTACLE"},
        {"capteur_gauche=obstacle",      "LEFT_OBSTACLE"},
        {"capteur_gauche=capteur1",      "LEFT_SENSOR1"},
        {"capteur_droit=capteur2",       "RIGHT_SENSOR2"},
        {"capteur_arriere_droit=capteur","REAR_RIGHT_SENSOR"},
        {"capteur_arr_gauche=obstacle AND capteur_arr_droit=obstacle", "BOTH_REAR_OBSTACLE"},
        {"capteur_arr_gauche=capteur AND capteur_arr_droit=capteur",   "BOTH_REAR_SENSOR"},
        {"demarrage",                    "DEMARRAGE"},
        {"delai_5s_apres_stop",          "DELAY_5S"},
        {"delai_3s_apres_stop",          "DELAY_3S"},
    };

    auto steps = ParkingDBManager::instance().getEtapesManoeuvre(maneuverType);
    for (const QVariantMap &step : steps) {
        QString cond = step.value("condition_capteur").toString().trimmed();
        QString msg  = step.value("message_guidance").toString().trimmed();
        int    dbNum = step.value("numero_etape").toInt();
        bool   stop  = step.value("est_stop").toBool();

        if (cond.isEmpty()) continue;
        QString evKey = condToEvent.value(cond, cond);

        if (!msg.isEmpty())
            m_dbSensorMessages[evKey] = msg;
        m_eventToDbStep[evKey]   = dbNum;
        m_dbStopRequired[evKey]  = stop;
    }
    qDebug() << "[DB-MSG] Loaded" << m_dbSensorMessages.size()
             << "sensor messages for maneuver:" << maneuverType;
}

QString ParkingWidget::dbMessageForEvent(const QString &event) const
{
    return m_dbSensorMessages.value(event);
}

// ═══════════════════════════════════════════════════════════════
// SENSOR-BASED ERROR DETECTION
//
// Rules (based on DB step data):
//  1. FRONT_OBSTACLE at any time        → Contact (obstacle hit)
//  2. Event expected at DB step N, but
//     student is already past step N+1  → Error (wrong sequence)
//  3. BOTH_REAR_OBSTACLE / BOTH_REAR_SENSOR fired while student
//     is still in early steps (< 6)     → Contact (reversed into obstacle)
// ═══════════════════════════════════════════════════════════════
void ParkingWidget::checkSensorErrors(const QString &event)
{
    if (!m_sessionActive) return;

    // ── Rule 1: Front obstacle = always a contact during parking ──
    if (event == "FRONT_OBSTACLE") {
        recordObstacleContact();
        return; // recordObstacleContact already shows message
    }

    // ── Rule 3: Rear obstacle/sensor fires too early ──
    // DB step 9 = BOTH_REAR_OBSTACLE, DB step 10 = BOTH_REAR_SENSOR
    // In C++ m_currentStep (0-based): step 8 = DB step 9
    if ((event == "BOTH_REAR_OBSTACLE" || event == "BOTH_REAR_SENSOR")
            && m_currentStep < 6) {
        // Rear sensors fired when student is still in early steps
        // = backed into rear obstacle too early
        recordObstacleContact();
        if (m_dbMsgLabel) {
            m_dbMsgLabel->setText(
                QString::fromUtf8("\xf0\x9f\x92\xa5 Contact \xe2\x80\x94 obstacle arri\xc3\xa8re d\xc3\xa9tect\xc3\xa9 trop t\xc3\xb4t !"));
            m_dbMsgLabel->setStyleSheet(
                "QLabel{background:#fef0f0;color:#d63031;font-size:12px;font-weight:bold;"
                "border-radius:10px;padding:10px 14px;"
                "border-left:4px solid #d63031;border-top:none;border-right:none;border-bottom:none;}");
            m_dbMsgLabel->setVisible(true);
        }
        return;
    }

    // ── Rule 2: Wrong sequence — event belongs to a step already passed ──
    int expectedDbStep = m_eventToDbStep.value(event, -1);
    if (expectedDbStep > 0) {
        // C++ m_currentStep is 0-based; DB step_number is 1-based
        // Allow 1 step of overlap (some events are warnings in adjacent phases)
        int currentDbStep = m_currentStep + 1; // convert to 1-based
        if (expectedDbStep < currentDbStep - 1) {
            // Event is for a step more than 1 behind current → wrong order
            recordError("sensor_sequence");
            if (m_dbMsgLabel) {
                m_dbMsgLabel->setText(
                    QString::fromUtf8(
                        "\xe2\x9a\xa0 Erreur de s\xc3\xa9quence \xe2\x80\x94 capteur inattendu \xc3\xa0 cette \xc3\xa9tape !"));
                m_dbMsgLabel->setStyleSheet(
                    "QLabel{background:#fef9e7;color:#f39c12;font-size:12px;font-weight:bold;"
                    "border-radius:10px;padding:10px 14px;"
                    "border-left:4px solid #f39c12;border-top:none;border-right:none;border-bottom:none;}");
                m_dbMsgLabel->setVisible(true);
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════
//  AI ASSISTANT PAGE  (Gemini 2.5-flash via curl.exe)
// ═══════════════════════════════════════════════════════════════════

QWidget* ParkingWidget::createAssistantPage()
{
    QWidget *page = new QWidget(this);
    page->setObjectName("aiPage");
    page->setStyleSheet("QWidget#aiPage{background:#f0f2f5;}");

    QVBoxLayout *pL = new QVBoxLayout(page);
    pL->setContentsMargins(0,0,0,0);
    pL->setSpacing(0);

    // ── Header bar ──────────────────────────────────────────────
    QFrame *hdr = new QFrame(page);
    hdr->setFixedHeight(56);
    hdr->setObjectName("aiHdr");
    hdr->setStyleSheet(
        "QFrame#aiHdr{"
        "  background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "    stop:0 #6c5ce7, stop:1 #a29bfe);"
        "  border:none;"
        "}");
    QHBoxLayout *hL = new QHBoxLayout(hdr);
    hL->setContentsMargins(16,0,16,0);
    hL->setSpacing(12);

    // Back button
    QPushButton *backBtn = new QPushButton(QString::fromUtf8("\xe2\x86\x90"), hdr);
    backBtn->setFixedSize(36,36);
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setStyleSheet(
        "QPushButton{background:rgba(255,255,255,0.2);color:white;border:none;"
        "border-radius:18px;font-size:16px;font-weight:bold;}"
        "QPushButton:hover{background:rgba(255,255,255,0.35);}"
    );
    connect(backBtn, &QPushButton::clicked, this, [this](){
        m_stack->setCurrentIndex(m_aiPrevPage);
    });
    hL->addWidget(backBtn);

    // Avatar
    // 🤖  U+1F916  →  UTF-8 F0 9F A4 96
    QLabel *av = new QLabel(QString::fromUtf8("\xf0\x9f\xa4\x96"), hdr);
    av->setFixedSize(38,38);
    av->setAlignment(Qt::AlignCenter);
    av->setStyleSheet("QLabel{font-size:22px;background:rgba(255,255,255,0.15);"
        "border-radius:19px;border:none;}");
    hL->addWidget(av);

    // Title + status
    QVBoxLayout *titleCol = new QVBoxLayout;
    titleCol->setSpacing(0);
    QLabel *titleLbl = new QLabel("Assistant Wino", hdr);
    titleLbl->setStyleSheet("QLabel{color:white;font-size:14px;font-weight:bold;"
        "background:transparent;border:none;}");
    titleCol->addWidget(titleLbl);
    QLabel *statusLbl = new QLabel(QString::fromUtf8("\xf0\x9f\xa4\x96  AI Gemini \xe2\x80\x94 Online"), hdr);
    statusLbl->setStyleSheet("QLabel{color:rgba(255,255,255,0.75);font-size:9px;"
        "background:transparent;border:none;}");
    titleCol->addWidget(statusLbl);
    hL->addLayout(titleCol);
    hL->addStretch();

    // API key settings button — ⚙️  U+2699 EF B8 8F
    QPushButton *settingsBtn = new QPushButton(QString::fromUtf8("\xe2\x9a\x99\xef\xb8\x8f"), hdr);
    settingsBtn->setFixedSize(36,36);
    settingsBtn->setCursor(Qt::PointingHandCursor);
    settingsBtn->setToolTip("Configure Gemini API Key");
    settingsBtn->setStyleSheet(
        "QPushButton{background:rgba(255,255,255,0.15);color:white;border:none;"
        "border-radius:18px;font-size:16px;}"
        "QPushButton:hover{background:rgba(255,255,255,0.3);}"
    );
    connect(settingsBtn, &QPushButton::clicked, this, &ParkingWidget::showAIApiKeyDialog);
    hL->addWidget(settingsBtn);

    pL->addWidget(hdr);

    // ── Chat scroll area ─────────────────────────────────────────
    m_aiScrollArea = new QScrollArea(page);
    m_aiScrollArea->setWidgetResizable(true);
    m_aiScrollArea->setFrameShape(QFrame::NoFrame);
    m_aiScrollArea->setStyleSheet(
        "QScrollArea{background:#f0f2f5;border:none;}"
        "QScrollBar:vertical{width:4px;background:transparent;}"
        "QScrollBar::handle:vertical{background:#b2bec3;border-radius:2px;}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}");

    m_aiChatContent = new QWidget();
    m_aiChatContent->setStyleSheet("QWidget{background:#f0f2f5;}");
    m_aiChatLayout = new QVBoxLayout(m_aiChatContent);
    m_aiChatLayout->setContentsMargins(16,16,16,16);
    m_aiChatLayout->setSpacing(10);
    m_aiChatLayout->addStretch();  // push bubbles to top

    m_aiScrollArea->setWidget(m_aiChatContent);
    pL->addWidget(m_aiScrollArea, 1);

    // Typing indicator (hidden by default)
    m_aiTypingLabel = new QLabel(QString::fromUtf8(
        "\xf0\x9f\xa4\x96  Assistant is typing..."), page);
    m_aiTypingLabel->setStyleSheet(
        "QLabel{color:#636e72;font-size:11px;font-style:italic;"
        "background:transparent;border:none;padding:4px 16px;}");
    m_aiTypingLabel->setVisible(false);
    pL->addWidget(m_aiTypingLabel);

    // ── Input bar ────────────────────────────────────────────────
    QFrame *inputBar = new QFrame(page);
    inputBar->setFixedHeight(60);
    inputBar->setObjectName("aiInputBar");
    inputBar->setStyleSheet(
        "QFrame#aiInputBar{background:white;border-top:1px solid #e2e8f0;}");
    QHBoxLayout *iL = new QHBoxLayout(inputBar);
    iL->setContentsMargins(12,8,12,8);
    iL->setSpacing(10);

    m_aiInput = new QLineEdit(inputBar);
    m_aiInput->setPlaceholderText("Ask the assistant your question...");
    m_aiInput->setStyleSheet(
        "QLineEdit{background:#f8f9fa;border:1.5px solid #e2e8f0;border-radius:20px;"
        "padding:8px 16px;font-size:12px;color:#2d3436;}"
        "QLineEdit:focus{border-color:#6c5ce7;background:white;}");
    iL->addWidget(m_aiInput, 1);

    // Send button — ▶  (filled)
    QPushButton *sendBtn = new QPushButton(QString::fromUtf8("\xe2\x96\xb6"), inputBar);
    sendBtn->setFixedSize(44,44);
    sendBtn->setCursor(Qt::PointingHandCursor);
    sendBtn->setStyleSheet(
        "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
        "  stop:0 #6c5ce7, stop:1 #a29bfe);"
        "  color:white;font-size:16px;border:none;border-radius:22px;}"
        "QPushButton:hover{background:#7d6ff0;}"
        "QPushButton:pressed{background:#5a4bd1;}"
    );
    connect(sendBtn,    &QPushButton::clicked, this, [this](){ sendToAI(m_aiInput->text()); });
    connect(m_aiInput, &QLineEdit::returnPressed, this, [this](){ sendToAI(m_aiInput->text()); });
    iL->addWidget(sendBtn);

    pL->addWidget(inputBar);

    // ── Welcome bubble ───────────────────────────────────────────
    QTimer::singleShot(0, this, [this](){
        addAIBubble(
            QString::fromUtf8(
                "Bonjour ! \xf0\x9f\x91\x8b Je suis l\xe2\x80\x99Assistant Wino, "
                "votre aide intelligent for the driving school.\n\n"
                "Posez-moi vos questions sur :\n"
                "\xf0\x9f\x93\x8b Le code de la route\n"
                "\xf0\x9f\x9a\x97 La conduite pratique\n"
                "\xf0\x9f\x93\x9d L\xe2\x80\x99examen du permis\n"
                "\xf0\x9f\x96\xbc\xef\xb8\x8f Les panneaux et signalisation\n\n"
                "Comment puis-je vous aider aujourd\xe2\x80\x99hui ?"),
            false
        );
    });

    // Load saved API key
    QSettings s("Wino", "ParkingAssistant");
    m_aiApiKey = s.value("gemini_api_key", "").toString();

    return page;
}

// ── Send a message to Gemini via curl.exe ──────────────────────────
void ParkingWidget::sendToAI(const QString &message)
{
    QString txt = message.trimmed();
    if (txt.isEmpty()) return;

    if (m_aiApiKey.isEmpty()) {
        showAIApiKeyDialog();
        if (m_aiApiKey.isEmpty()) {
            addAIBubble(
                QString::fromUtf8(
                    "\xe2\x9a\xa0\xef\xb8\x8f Please configure your Gemini API key.\n"
                    "Click the \xe2\x9a\x99\xef\xb8\x8f button in the header."),
                false);
            return;
        }
    }

    m_aiInput->clear();
    addAIBubble(txt, true);  // show user message immediately

    // Build Gemini request JSON (multi-turn)
    QJsonObject userPart;
    userPart["role"] = "user";
    QJsonArray userParts;
    QJsonObject textObj;
    textObj["text"] = txt;
    userParts.append(textObj);
    userPart["parts"] = userParts;
    m_aiHistory.append(userPart);

    // System instruction
    QString sysPrompt = m_isMoniteur ?
        QString::fromUtf8(
            "You are the Wino Assistant, an intelligent chatbot integrated in "
            "the Wino Smart Driving School parking module in Tunisia. "
            "You help with: managing driving sessions, Tunisian highway code, "
            "autonomous parking maneuvers (creneau, bataille, epi, demi-tour), "
            "sensor troubleshooting, and student progress. "
            "Always respond in English clearly and professionally. Use emojis.") :
        QString::fromUtf8(
            "You are the Wino Assistant, an intelligent chatbot integrated in "
            "the Wino Smart Driving School parking module in Tunisia. "
            "You help with: Tunisian highway code, practical driving tips, "
            "parking maneuvers (parallel, perpendicular, diagonal, reverse), "
            "exam preparation, and driving school procedures. "
            "Always respond in English in an encouraging manner. Use emojis.");

    QJsonObject sysInstruction;
    QJsonArray sysParts;
    QJsonObject sysText;
    sysText["text"] = sysPrompt;
    sysParts.append(sysText);
    sysInstruction["parts"] = sysParts;

    QJsonObject genConfig;
    genConfig["temperature"] = 0.7;
    genConfig["maxOutputTokens"] = 1024;

    QJsonObject body;
    body["contents"]           = m_aiHistory;
    body["systemInstruction"]  = sysInstruction;
    body["generationConfig"]   = genConfig;

    QByteArray jsonBytes = QJsonDocument(body).toJson(QJsonDocument::Compact);

    // Write payload to temp file
    if (m_aiTmpPayload) { m_aiTmpPayload->deleteLater(); m_aiTmpPayload = nullptr; }
    m_aiTmpPayload = new QTemporaryFile(this);
    m_aiTmpPayload->setAutoRemove(true);
    if (!m_aiTmpPayload->open()) {
        addAIBubble(QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f Could not create temp file."), false);
        return;
    }
    m_aiTmpPayload->write(jsonBytes);
    m_aiTmpPayload->flush();

    QString url = QString(
        "https://generativelanguage.googleapis.com/v1beta/models/"
        "gemini-2.5-flash:generateContent");

    QStringList args;
    args << "-s" << "-X" << "POST"
         << "-H" << "Content-Type: application/json"
         << "-H" << ("x-goog-api-key: " + m_aiApiKey)
         << "--data-binary" << ("@" + m_aiTmpPayload->fileName())
         << url;

    if (m_aiProc) { m_aiProc->kill(); m_aiProc->deleteLater(); m_aiProc = nullptr; }
    m_aiProc = new QProcess(this);
    m_aiProc->setProcessChannelMode(QProcess::MergedChannels);
    connect(m_aiProc, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus){ onAIReply(); });

    m_aiTypingLabel->setVisible(true);
    m_aiProc->start("C:/Windows/System32/curl.exe", args);
}

// ── Parse Gemini reply ─────────────────────────────────────────────
void ParkingWidget::onAIReply()
{
    m_aiTypingLabel->setVisible(false);

    if (!m_aiProc) return;
    QByteArray raw = m_aiProc->readAll();
    m_aiProc->deleteLater(); m_aiProc = nullptr;
    if (m_aiTmpPayload) { m_aiTmpPayload->close(); m_aiTmpPayload->deleteLater(); m_aiTmpPayload = nullptr; }

    QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (doc.isNull() || !doc.isObject()) {
        addAIBubble(QString::fromUtf8(
            "\xe2\x9a\xa0\xef\xb8\x8f Network error — check your internet connection."), false);
        return;
    }

    QJsonObject obj = doc.object();

    // Check Gemini error
    if (obj.contains("error")) {
        QString errMsg = obj["error"].toObject()["message"].toString();
        int code = obj["error"].toObject()["code"].toInt();
        if (code == 401 || code == 403) {
            addAIBubble(QString::fromUtf8(
                "\xe2\x9a\xa0\xef\xb8\x8f Invalid API key. Click \xe2\x9a\x99\xef\xb8\x8f to reconfigure."), false);
            m_aiApiKey.clear();
            QSettings s("Wino", "ParkingAssistant");
            s.remove("gemini_api_key");
        } else {
            addAIBubble(QString::fromUtf8("\xe2\x9a\xa0\xef\xb8\x8f ") + errMsg, false);
        }
        return;
    }

    // Extract text
    QString responseText;
    QJsonArray candidates = obj["candidates"].toArray();
    if (!candidates.isEmpty()) {
        QJsonArray parts = candidates[0].toObject()["content"].toObject()["parts"].toArray();
        if (!parts.isEmpty())
            responseText = parts[0].toObject()["text"].toString();
    }

    if (responseText.isEmpty()) {
        addAIBubble(QString::fromUtf8(
            "\xe2\x9a\xa0\xef\xb8\x8f Empty response from Gemini."), false);
        return;
    }

    // Save assistant reply to history
    QJsonObject modelPart;
    modelPart["role"] = "model";
    QJsonArray modelParts;
    QJsonObject mtObj;
    mtObj["text"] = responseText;
    modelParts.append(mtObj);
    modelPart["parts"] = modelParts;
    m_aiHistory.append(modelPart);

    addAIBubble(responseText, false);
}

// ── Create a chat bubble widget and add it to the chat layout ──────
void ParkingWidget::addAIBubble(const QString &text, bool isUser)
{
    QWidget *row = new QWidget(m_aiChatContent);
    QHBoxLayout *rL = new QHBoxLayout(row);
    rL->setContentsMargins(0,0,0,0);
    rL->setSpacing(8);

    if (isUser) {
        // User bubble — right-aligned, green gradient
        rL->addStretch();

        QLabel *bubble = new QLabel(text, row);
        bubble->setWordWrap(true);
        bubble->setMaximumWidth(320);
        bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
        bubble->setStyleSheet(
            "QLabel{"
            "  background:qlineargradient(x1:0,y1:0,x2:1,y2:1,"
            "    stop:0 #00b894, stop:1 #00a884);"
            "  color:white; font-size:12px; border-radius:18px;"
            "  border-bottom-right-radius:4px;"
            "  padding:10px 14px; border:none;"
            "}");
        rL->addWidget(bubble);
    } else {
        // Assistant bubble — left-aligned, white
        // 🤖 avatar
        QLabel *av = new QLabel(QString::fromUtf8("\xf0\x9f\xa4\x96"), row);
        av->setFixedSize(32,32);
        av->setAlignment(Qt::AlignCenter);
        av->setStyleSheet("QLabel{font-size:16px;background:#6c5ce7;"
            "border-radius:16px;border:none;}");
        rL->addWidget(av, 0, Qt::AlignTop);

        QLabel *bubble = new QLabel(text, row);
        bubble->setWordWrap(true);
        bubble->setMaximumWidth(340);
        bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
        bubble->setStyleSheet(
            "QLabel{"
            "  background:white; color:#2d3436; font-size:12px;"
            "  border-radius:18px; border-top-left-radius:4px;"
            "  padding:10px 14px;"
            "  border:1px solid #e2e8f0;"
            "}");
        rL->addWidget(bubble);
        rL->addStretch();
    }

    // Insert before the stretch at the end
    m_aiChatLayout->insertWidget(m_aiChatLayout->count() - 1, row);

    // Scroll to bottom
    QTimer::singleShot(50, this, [this](){
        if (m_aiScrollArea)
            m_aiScrollArea->verticalScrollBar()->setValue(
                m_aiScrollArea->verticalScrollBar()->maximum());
    });
}

// ── API Key dialog ─────────────────────────────────────────────────
void ParkingWidget::showAIApiKeyDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle("Gemini API Key");
    dlg.setFixedWidth(420);
    dlg.setStyleSheet("QDialog{background:white;}");

    QVBoxLayout *vL = new QVBoxLayout(&dlg);
    vL->setContentsMargins(24,24,24,24);
    vL->setSpacing(14);

    // Title
    QLabel *title = new QLabel(
        QString::fromUtf8("\xf0\x9f\xa4\x96  Configure Gemini API Key"), &dlg);
    title->setStyleSheet("QLabel{font-size:15px;font-weight:bold;color:#2d3436;"
        "background:transparent;border:none;}");
    vL->addWidget(title);

    // Info
    QLabel *info = new QLabel(
        QString::fromUtf8(
            "Get a free API key at:\n"
            "https://aistudio.google.com/app/apikey\n\n"
            "The key is stored locally on your device only."), &dlg);
    info->setWordWrap(true);
    info->setStyleSheet("QLabel{color:#636e72;font-size:11px;"
        "background:#f8f9fa;border-radius:8px;padding:10px;border:none;}");
    vL->addWidget(info);

    // Input
    QLineEdit *keyEdit = new QLineEdit(&dlg);
    keyEdit->setPlaceholderText("AIza...");
    keyEdit->setText(m_aiApiKey);
    keyEdit->setEchoMode(QLineEdit::Password);
    keyEdit->setStyleSheet(
        "QLineEdit{background:#f8f9fa;border:1.5px solid #e2e8f0;border-radius:10px;"
        "padding:10px 14px;font-size:12px;color:#2d3436;font-family:'Consolas';}"
        "QLineEdit:focus{border-color:#6c5ce7;}");
    vL->addWidget(keyEdit);

    // Show/hide toggle
    QCheckBox *showKey = new QCheckBox("Show key", &dlg);
    showKey->setStyleSheet("QCheckBox{color:#636e72;font-size:11px;background:transparent;border:none;}");
    connect(showKey, &QCheckBox::toggled, this, [keyEdit](bool on){
        keyEdit->setEchoMode(on ? QLineEdit::Normal : QLineEdit::Password);
    });
    vL->addWidget(showKey);

    // Buttons
    QHBoxLayout *btnL = new QHBoxLayout;
    QPushButton *cancelBtn = new QPushButton("Cancel", &dlg);
    cancelBtn->setStyleSheet(
        "QPushButton{background:#f0f2f5;color:#636e72;border:none;"
        "border-radius:10px;padding:10px 20px;font-size:12px;}"
        "QPushButton:hover{background:#e2e8f0;}");
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    btnL->addWidget(cancelBtn);

    QPushButton *saveBtn = new QPushButton("Save & Connect", &dlg);
    saveBtn->setStyleSheet(
        "QPushButton{background:qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "  stop:0 #6c5ce7, stop:1 #a29bfe);"
        "  color:white;border:none;border-radius:10px;padding:10px 20px;"
        "  font-size:12px;font-weight:bold;}"
        "QPushButton:hover{background:#7d6ff0;}"
        "QPushButton:pressed{background:#5a4bd1;}");
    connect(saveBtn, &QPushButton::clicked, this, [&](){
        m_aiApiKey = keyEdit->text().trimmed();
        QSettings s("Wino", "ParkingAssistant");
        s.setValue("gemini_api_key", m_aiApiKey);
        dlg.accept();
    });
    btnL->addWidget(saveBtn);
    vL->addLayout(btnL);

    dlg.exec();
}
