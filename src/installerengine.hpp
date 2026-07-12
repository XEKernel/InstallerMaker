#pragma once

#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

/// Core installer / uninstaller engine.
///
/// Install:
///   - Read embedded package.json
///   - Resolve path variables ($LOCALAPPDATA$, $PROGRAMFILES$)
///   - Copy :/package/files/ → target directory
///   - Write registry entries
///   - Create Desktop / StartMenu shortcuts (via Shortcut)
///   - Optionally restart elevated
///
/// Uninstall:
///   - Remove files from install directory
///   - Remove shortcuts
///   - Remove registry entries
///
/// The engine is designed to be moved to a worker thread via moveToThread().
class InstallerEngine : public QObject {
    Q_OBJECT

public:
    explicit InstallerEngine(QObject* parent = nullptr);
    ~InstallerEngine() override = default;

    // ── Package ──────────────────────────────────────────────────────────
    bool loadPackage();
    const QJsonObject& package() const { return m_package; }

    // ── Static helpers ───────────────────────────────────────────────────
    static QString resolvePath(const QString& raw);
    static bool    canWriteToDir(const QString& dirPath);
    static bool    elevateSelf(const QString& installDir,
                               const QJsonObject& package);

    /// Look up an installed application's location from the Windows registry.
    static QString findInstallLocation(const QString& appName);

    // ── Accessors ────────────────────────────────────────────────────────
    QString appName()           const;
    QString version()           const;
    QString publisher()         const;
    QString defaultInstallPath()const;
    QString mainExecutable()    const;
    QString licenseText()       const;
    QString finishMessage()     const;
    QString startMenuFolder()   const;
    QString welcomeText()       const;
    QString appIcon()           const;
    QString description()       const;
    QString supportUrl()        const;
    QString updateUrl()         const;
    bool    requireAdmin()      const;
    bool    autoStart()         const;
    bool    requireRestart()    const;
    bool    runAfterInstall()   const;

public slots:
    /// Execute full installation (call from worker thread).
    void install(const QString& installDir);

    /// Execute full uninstallation (call from worker thread).
    void uninstall(const QString& installDir);

signals:
    void progressChanged(int percent);
    void statusChanged(const QString& message);
    void finished(bool success, const QString& errorText);

private:
    // ── Install helpers ──────────────────────────────────────────────────
    void copyFiles(const QString& installDir, int& step, int totalSteps);
    void writeRegistry(const QString& installDir);
    void createShortcuts(const QString& installDir);
    void writeUninstallInfo(const QString& installDir);
    void deployUninstaller(const QString& installDir);

    // ── Uninstall helpers ────────────────────────────────────────────────
    void removeFiles(const QString& installDir, int& step, int totalSteps);
    void removeShortcuts(const QString& appName);
    void removeRegistry(const QString& appName);

    QJsonObject m_package;
};
