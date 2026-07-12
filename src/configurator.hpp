#pragma once

#include <QDialog>
#include <QList>
#include <QPair>

class QLineEdit;
class QComboBox;
class QCheckBox;
class QTextEdit;
class QPushButton;
class QProgressBar;
class QVBoxLayout;

/// GUI tool to configure the installer package and trigger a build.
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
    void onGenerate();

private:
    void setupUi();
    void buildBasicGroup(QVBoxLayout* main);
    void buildInstallGroup(QVBoxLayout* main);
    void buildFilesGroup(QVBoxLayout* main);
    void buildLicenseGroup(QVBoxLayout* main);
    void buildAdvancedGroup(QVBoxLayout* main);
    void buildRegistryGroup(QVBoxLayout* main);
    void buildOutputGroup(QVBoxLayout* main);
    void buildActionBar(QVBoxLayout* main);

    bool generatePackageJson(const QString& packageDir);
    bool runBuild(const QString& packageDir, const QString& outputDir);

    // ── Basic info ───────────────────────────────────────────────────────
    QLineEdit*   m_nameEdit       = nullptr;
    QLineEdit*   m_versionEdit    = nullptr;
    QLineEdit*   m_publisherEdit  = nullptr;

    // ── Install ──────────────────────────────────────────────────────────
    QComboBox*   m_rootCombo      = nullptr;
    QLineEdit*   m_folderEdit     = nullptr;
    QLineEdit*   m_exeEdit        = nullptr;

    // ── Files & license ──────────────────────────────────────────────────
    QLineEdit*   m_filesEdit      = nullptr;
    QLineEdit*   m_licenseEdit    = nullptr;

    // ── Shortcuts ────────────────────────────────────────────────────────
    QCheckBox*   m_desktopBox     = nullptr;
    QCheckBox*   m_startMenuBox   = nullptr;

    // ── Advanced ─────────────────────────────────────────────────────────
    QLineEdit*   m_startMenuFolderEdit = nullptr;
    QCheckBox*   m_requireAdminBox     = nullptr;
    QLineEdit*   m_supportUrlEdit      = nullptr;
    QLineEdit*   m_updateUrlEdit       = nullptr;
    QLineEdit*   m_iconEdit            = nullptr;
    QLineEdit*   m_uninstallIconEdit   = nullptr;
    QLineEdit*   m_welcomeTextEdit     = nullptr;
    QLineEdit*   m_descriptionEdit      = nullptr;
    QCheckBox*   m_autoStartBox         = nullptr;
    QCheckBox*   m_requireRestartBox    = nullptr;
    QCheckBox*   m_runAfterInstallBox   = nullptr;

    // ── Extra registry (dynamic rows) ────────────────────────────────────
    QVBoxLayout* m_regEntriesLayout = nullptr;
    struct RegRow {
        QLineEdit* key;
        QLineEdit* value;
    };
    QList<RegRow> m_regRows;

    // ── Finish ───────────────────────────────────────────────────────────
    QLineEdit*   m_finishEdit     = nullptr;

    // ── Output ───────────────────────────────────────────────────────────
    QLineEdit*   m_outputEdit     = nullptr;

    // ── Action ───────────────────────────────────────────────────────────
    QPushButton* m_generateBtn    = nullptr;
    QProgressBar*m_progressBar    = nullptr;
    QTextEdit*   m_logEdit        = nullptr;
};
