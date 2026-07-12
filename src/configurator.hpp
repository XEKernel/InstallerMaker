#pragma once

#include <QDialog>
#include <QList>

class QLineEdit;
class QComboBox;
class QCheckBox;
class QTextEdit;
class QPushButton;
class QProgressBar;
class QVBoxLayout;
class QTabWidget;
class QSpinBox;

class Configurator : public QDialog {
    Q_OBJECT
public:
    explicit Configurator(QWidget* parent = nullptr);

private slots:
    void onBrowseFiles();
    void onBrowseLicense();
    void onBrowseIcon();
    void onBrowseOutput();
    void onAddRegEntry();
    void onAddShortcut();
    void onAddFileAssoc();
    void onGenerate();

private:
    void setupUi();

    // Tab builders
    QWidget* buildTabBasic();
    QWidget* buildTabInstall();
    QWidget* buildTabFiles();
    QWidget* buildTabShortcuts();
    QWidget* buildTabAppearance();
    QWidget* buildTabRegistry();
    QWidget* buildTabUninstall();
    QWidget* buildTabEnvironment();
    QWidget* buildTabAssociations();
    QWidget* buildTabFinish();
    QWidget* buildTabAdvanced();
    QWidget* buildTabOutput();

    bool generatePackageJson(const QString& packageDir);
    bool runBuild(const QString& packageDir, const QString& outputDir);

    // ── Tab 1: Basic ─────────────────────────────────────────────────────
    QLineEdit* m_name       = nullptr;
    QLineEdit* m_shortName  = nullptr;
    QLineEdit* m_version    = nullptr;
    QLineEdit* m_publisher  = nullptr;
    QLineEdit* m_copyright  = nullptr;
    QLineEdit* m_website    = nullptr;
    QLineEdit* m_email      = nullptr;

    // ── Tab 2: Install ───────────────────────────────────────────────────
    QComboBox* m_rootCombo  = nullptr;
    QLineEdit* m_folder     = nullptr;
    QLineEdit* m_exe        = nullptr;
    QLineEdit* m_license    = nullptr;
    QCheckBox* m_reqAdmin   = nullptr;
    QCheckBox* m_allowPath  = nullptr;

    // ── Tab 3: Files ─────────────────────────────────────────────────────
    QLineEdit* m_filesDir   = nullptr;
    QComboBox* m_fileMode   = nullptr;
    QComboBox* m_overwrite  = nullptr;
    QLineEdit* m_exclude    = nullptr;

    // ── Tab 4: Shortcuts ─────────────────────────────────────────────────
    struct ShortcutRow { QLineEdit* target; QComboBox* location; QLineEdit* name;
        QLineEdit* desc; QLineEdit* args; QLineEdit* wd; QLineEdit* icon;
        QLineEdit* smFolder; QCheckBox* admin; };
    QList<ShortcutRow> m_scRows;
    QVBoxLayout* m_scLayout = nullptr;

    // ── Tab 5: Appearance ────────────────────────────────────────────────
    QLineEdit* m_icon       = nullptr;
    QLineEdit* m_setupIcon  = nullptr;
    QLineEdit* m_uninstIcon = nullptr;
    QLineEdit* m_banner     = nullptr;
    QLineEdit* m_bgColor    = nullptr;
    QLineEdit* m_welcomeTitle  = nullptr;
    QLineEdit* m_welcomeText   = nullptr;
    QLineEdit* m_finishTitle   = nullptr;
    QLineEdit* m_finishMsg     = nullptr;

    // ── Tab 6: Registry ──────────────────────────────────────────────────
    struct RegRow { QLineEdit* key; QLineEdit* value; };
    QList<RegRow> m_regRows;
    QVBoxLayout* m_regLayout = nullptr;

    // ── Tab 7: Uninstall ─────────────────────────────────────────────────
    QLineEdit* m_uninstString = nullptr;
    QLineEdit* m_quietUninst  = nullptr;
    QLineEdit* m_uninstDispIcon = nullptr;
    QSpinBox*  m_estSize      = nullptr;
    QCheckBox* m_noModify     = nullptr;
    QCheckBox* m_noRepair     = nullptr;
    QLineEdit* m_helpLink     = nullptr;

    // ── Tab 8: Environment ───────────────────────────────────────────────
    QLineEdit* m_userPath     = nullptr;
    QLineEdit* m_userVars     = nullptr;
    QLineEdit* m_sysPath      = nullptr;
    QLineEdit* m_sysVars      = nullptr;

    // ── Tab 9: File associations ─────────────────────────────────────────
    struct AssocRow { QLineEdit* ext; QLineEdit* desc; QLineEdit* cmd;
        QLineEdit* icon; QLineEdit* progId; };
    QList<AssocRow> m_assocRows;
    QVBoxLayout* m_assocLayout = nullptr;

    // ── Tab 10: Finish ───────────────────────────────────────────────────
    QCheckBox* m_runProg    = nullptr;
    QLineEdit* m_runArgs    = nullptr;
    QLineEdit* m_openReadme = nullptr;
    QCheckBox* m_restart    = nullptr;

    // ── Tab 11: Advanced ─────────────────────────────────────────────────
    QCheckBox* m_createUninst = nullptr;
    QCheckBox* m_enableLog    = nullptr;
    QCheckBox* m_silentInst   = nullptr;
    QLineEdit* m_minOs        = nullptr;
    QCheckBox* m_64bitOnly    = nullptr;

    // ── Tab 12: Output + action ──────────────────────────────────────────
    QLineEdit* m_output       = nullptr;
    QPushButton* m_generateBtn = nullptr;
    QProgressBar* m_progressBar = nullptr;
    QTextEdit* m_log           = nullptr;
};
