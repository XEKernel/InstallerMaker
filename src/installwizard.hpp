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
class QHBoxLayout;

/// Installer / uninstaller dialog.
///
/// Two UI styles:
///   - wizard: left step panel + right stacked pages
///   - oneclick: single page with all options, inline progress
class InstallWizard : public QDialog {
    Q_OBJECT

public:
    enum Mode { Install, Uninstall };
    enum UiStyle { Wizard, OneClick };

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
    void onOneClickInstall();
    void onShowLicense();

private:
    // ── Wizard mode ──────────────────────────────────────────────────────
    void setupWizardUi();
    void navigateTo(int step);
    void refreshButtons();
    void refreshStepIndicator();
    QWidget* buildWelcomePage();
    QWidget* buildLicensePage();
    QWidget* buildPathPage();
    QWidget* buildProgressPage();
    QWidget* buildFinishPage();

    // ── One-click mode ───────────────────────────────────────────────────
    void setupOneClickUi();

    // ── Uninstall pages (wizard only) ────────────────────────────────────
    QWidget* buildUninstallConfirmPage();
    QWidget* buildUninstallProgressPage();
    QWidget* buildUninstallFinishPage();

    // ── Shared ───────────────────────────────────────────────────────────
    void startInstallation(const QString& path);

    Mode          m_mode;
    UiStyle       m_style;
    QJsonObject   m_package;
    QString       m_uninstallDir;
    int           m_currentStep = 0;

    QStringList   m_installSteps;
    QStringList   m_uninstallSteps;

    // ── Wizard widgets ───────────────────────────────────────────────────
    QVBoxLayout*  m_stepLayout   = nullptr;
    QStackedWidget* m_stack      = nullptr;
    QPushButton*  m_backBtn      = nullptr;
    QPushButton*  m_nextBtn      = nullptr;
    QPushButton*  m_cancelBtn    = nullptr;
    QLineEdit*    m_pathEdit     = nullptr;
    QCheckBox*    m_licenseBox   = nullptr;
    QProgressBar* m_progressBar  = nullptr;
    QLabel*       m_statusLabel  = nullptr;
    QCheckBox*    m_runCheckBox  = nullptr;
    QTextEdit*    m_detailLog    = nullptr;  // progress page detail log

    // ── One-click widgets ────────────────────────────────────────────────
    QWidget*      m_oneClickPage = nullptr;
    QLineEdit*    m_ocPathEdit   = nullptr;
    QCheckBox*    m_ocLicenseBox = nullptr;
    QPushButton*  m_ocLicenseBtn = nullptr;
    QCheckBox*    m_ocDesktopBox = nullptr;
    QCheckBox*    m_ocStartMenuBox = nullptr;
    QCheckBox*    m_ocAutoStartBox = nullptr;
    QProgressBar* m_ocProgressBar = nullptr;
    QLabel*       m_ocStatusLabel = nullptr;
    QPushButton*  m_ocInstallBtn = nullptr;
};
