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

class InstallWizard : public QDialog {
    Q_OBJECT
public:
    enum Mode { Install, Uninstall };
    enum Style { WizardStyle, OneClick };

    InstallWizard(Mode mode, const QJsonObject& package,
                  const QString& uninstallDir = {}, QWidget* parent = nullptr);

    void done(int result) override;

private slots:
    void onWizardBack();
    void onWizardNext();
    void onProgress(int pct);
    void onStatus(const QString& msg);
    void onEngineFinished(bool ok, const QString& err);
    void onOneClickGo();
    void onShowLicense();

private:
    void setupWizard();
    void setupOneClick();
    void startInstall(const QString& dir);
    void refreshWizardNav();
    void refreshSteps();

    QWidget* pageWelcome();
    QWidget* pageLicense();
    QWidget* pagePath();
    QWidget* pageProgress();
    QWidget* pageFinish();
    QWidget* pageUninstConfirm();
    QWidget* pageUninstProgress();
    QWidget* pageUninstFinish();

    // Data
    Mode        m_mode;
    Style       m_style;
    QJsonObject m_pkg;
    QString     m_uninstDir;
    int         m_step = 0;
    QStringList m_steps;

    // Wizard widgets
    QStackedWidget* m_stack      = nullptr;
    QVBoxLayout*    m_stepLayout = nullptr;
    QPushButton*    m_btnBack    = nullptr;
    QPushButton*    m_btnNext    = nullptr;
    QPushButton*    m_btnCancel  = nullptr;
    QLineEdit*      m_editPath   = nullptr;
    QCheckBox*      m_chkLicense = nullptr;
    QProgressBar*   m_bar        = nullptr;
    QLabel*         m_lblStatus  = nullptr;
    QCheckBox*      m_chkRun     = nullptr;
    QTextEdit*      m_log        = nullptr;

    // One-click widgets
    QLineEdit*      m_ocPath     = nullptr;
    QCheckBox*      m_ocLicense  = nullptr;
    QPushButton*    m_ocBtn      = nullptr;
    QProgressBar*   m_ocBar      = nullptr;
    QLabel*         m_ocStatus   = nullptr;
};
