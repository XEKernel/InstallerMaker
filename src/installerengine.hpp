#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>

class InstallerEngine : public QObject {
    Q_OBJECT

public:
    explicit InstallerEngine(QObject* parent = nullptr);
    ~InstallerEngine() override = default;

    bool loadPackage();
    const QJsonObject& package() const { return m_package; }

    // ── Static helpers ───────────────────────────────────────────────────
    static QString resolvePath(const QString& raw);
    static bool    canWriteToDir(const QString& dirPath);
    static bool    elevateSelf(const QString& installDir,
                               const QJsonObject& package);
    static QString findInstallLocation(const QString& appName);
    static bool    checkOsVersion(const QString& minVer);

    // ── Accessors (installer.*) ──────────────────────────────────────────
    QString appName()             const;
    QString shortName()           const;
    QString version()             const;
    QString publisher()           const;
    QString copyright()           const;
    QString website()             const;
    QString defaultInstallPath()  const;
    QString mainExecutable()      const;
    QString licenseText()         const;
    QString welcomeTitle()        const;
    QString welcomeText()         const;
    QString finishTitle()         const;
    QString finishMessage()       const;
    QString iconPath()            const;
    QString bannerImage()         const;
    bool    requiresAdmin()       const;
    bool    allowChangePath()     const;

    // ── files_strategy ───────────────────────────────────────────────────
    QString fileMode()            const;  // "mirror" | "mapping"
    QString fileOverwrite()       const;  // "ask"|"always"|"never"
    QStringList fileExclude()     const;

    // ── uninstall ────────────────────────────────────────────────────────
    QString uninstallString()     const;
    QString displayIcon()         const;
    bool    uninstallNoModify()   const;
    bool    uninstallNoRepair()   const;

    // ── finish_actions ───────────────────────────────────────────────────
    bool    finishRunProgram()    const;
    QString finishRunArgs()       const;
    QString finishOpenReadme()    const;
    bool    finishRestartRequired() const;

    // ── advanced ─────────────────────────────────────────────────────────
    bool    advancedCreateUninstaller() const;
    bool    advancedEnableLogging()     const;
    bool    advancedSilentInstall()     const;
    QString advancedMinOsVersion()      const;
    bool    advanced64BitOnly()         const;

public slots:
    void install(const QString& installDir);
    void uninstall(const QString& installDir);

signals:
    void progressChanged(int percent);
    void statusChanged(const QString& message);
    void finished(bool success, const QString& errorText);

private:
    // Install steps
    void copyFiles(const QString& installDir, int& step, int totalSteps);
    void writeRegistry(const QString& installDir);
    void createShortcuts(const QString& installDir);
    void writeEnvironment(const QString& installDir);
    void writeFileAssociations(const QString& installDir);
    void deployUninstaller(const QString& installDir);
    void writeUninstallInfo(const QString& installDir);

    // Uninstall steps  
    void removeFiles(const QString& installDir, int& step, int totalSteps);
    void removeShortcuts(const QString& appName);
    void removeRegistry(const QString& appName);
    void removeEnvironment(const QString& appName);
    void removeFileAssociations();
    void removeAutoStart(const QString& appName);

    QJsonObject m_package;
};
