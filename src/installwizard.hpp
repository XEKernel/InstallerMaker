#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QStringList>

class QLabel;
class QLineEdit;
class QCheckBox;
class QTextEdit;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

/// Single-window installer / uninstaller dialog.
///
/// Layout:
///   ┌──────────┬──────────────────────────┐
///   │  step    │   content area           │
///   │  list    │   (QStackedWidget)       │
///   │  (left)  │                          │
///   ├──────────┴──────────────────────────┤
///   │       [Back]  [Next/Install]  [Cancel] │
///   └─────────────────────────────────────┘
class InstallWizard : public QDialog {
    Q_OBJECT

public:
    enum Mode { Install, Uninstall };

    InstallWizard(Mode mode,
                  const QJsonObject& package,
                  const QString& uninstallDir = {},
                  QWidget* parent = nullptr);

    void done(int result) override;

private slots:
    void onBack();
    void onNext();
    void onProgressChanged(int percent);
    void onStatusChanged(const QString& text);
    void onFinished(bool success, const QString& error);

private:
    void setupUi();
    void navigateTo(int step);
    void refreshButtons();
    void refreshStepIndicator();
    void startInstallation(const QString& path);
    void startUninstallation();

    // ── Page builders ────────────────────────────────────────────────────
    QWidget* buildWelcomePage();
    QWidget* buildLicensePage();
    QWidget* buildPathPage();
    QWidget* buildProgressPage();
    QWidget* buildFinishPage();
    QWidget* buildUninstallConfirmPage();
    QWidget* buildUninstallProgressPage();
    QWidget* buildUninstallFinishPage();

    // ── Data ─────────────────────────────────────────────────────────────
    Mode          m_mode;
    QJsonObject   m_package;
    QString       m_uninstallDir;
    int           m_currentStep = 0;

    // step names (ordered)
    QStringList   m_installSteps;
    QStringList   m_uninstallSteps;

    // ── Left panel ───────────────────────────────────────────────────────
    QVBoxLayout*  m_stepLayout   = nullptr;
    // per-step labels: [iconLabel, textLabel]

    // ── Right panel ──────────────────────────────────────────────────────
    QStackedWidget* m_stack      = nullptr;

    // ── Bottom buttons ───────────────────────────────────────────────────
    QPushButton*  m_backBtn      = nullptr;
    QPushButton*  m_nextBtn      = nullptr;
    QPushButton*  m_cancelBtn    = nullptr;

    // ── Page 1 (license) widgets ─────────────────────────────────────────
    QCheckBox*    m_licenseBox   = nullptr;

    // ── Page 2 (install path) widgets ────────────────────────────────────
    QLineEdit*    m_pathEdit     = nullptr;

    // ── Page 3 / uninstall-1 (progress) widgets ──────────────────────────
    QProgressBar* m_progressBar  = nullptr;
    QLabel*       m_statusLabel  = nullptr;

    // ── Page 4 (finish) widgets ──────────────────────────────────────────
    QCheckBox*    m_runCheckBox  = nullptr;
};
