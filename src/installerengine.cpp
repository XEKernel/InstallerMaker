#include "installerengine.hpp"
#include "shortcut.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <shellapi.h>
#endif

// ── Constructor ──────────────────────────────────────────────────────────────

InstallerEngine::InstallerEngine(QObject* parent)
    : QObject(parent) {}

// ── Package loading ──────────────────────────────────────────────────────────

bool InstallerEngine::loadPackage()
{
    QFile f(":/package/package.json");
    if (!f.open(QIODevice::ReadOnly)) {
        emit finished(false, tr("无法读取内嵌的 package.json"));
        return false;
    }
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError) {
        emit finished(false, tr("package.json 解析失败: %1").arg(err.errorString()));
        return false;
    }
    m_package = doc.object();
    return true;
}

// ── Path resolution ─────────────────────────────────────────────────────────

QString InstallerEngine::resolvePath(const QString& raw)
{
    QString s = raw;
    s.replace(QStringLiteral("$LOCALAPPDATA$"),
              QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    s.replace(QStringLiteral("$PROGRAMFILES$"),
              QDir::fromNativeSeparators(qEnvironmentVariable("ProgramFiles")));
    return QDir::toNativeSeparators(s);
}

// ── Writable check / Elevation ──────────────────────────────────────────────

bool InstallerEngine::canWriteToDir(const QString& dirPath)
{
    QDir dir;
    if (!dir.mkpath(dirPath)) return false;
    QTemporaryFile tmp(dirPath + QStringLiteral("/.wr_test_XXXXXX"));
    return tmp.open();
}

bool InstallerEngine::elevateSelf(const QString& installDir,
                                  const QJsonObject& package)
{
#ifdef Q_OS_WIN
    QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                       + QStringLiteral("/installer_state.json");
    QJsonObject state;
    state[QStringLiteral("install_dir")] = installDir;
    state[QStringLiteral("package")]     = package;
    QFile sf(tempPath);
    if (!sf.open(QIODevice::WriteOnly)) return false;
    sf.write(QJsonDocument(state).toJson(QJsonDocument::Compact));
    sf.close();

    const QString exePath = QCoreApplication::applicationFilePath();
    const QString params  = QStringLiteral("--install-silent \"%1\"").arg(tempPath);
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize       = sizeof(sei);
    sei.fMask        = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb       = L"runas";
    sei.lpFile       = reinterpret_cast<LPCWSTR>(exePath.utf16());
    sei.lpParameters = reinterpret_cast<LPCWSTR>(params.utf16());
    sei.nShow        = SW_NORMAL;

    if (!ShellExecuteExW(&sei)) return false;
    WaitForSingleObject(sei.hProcess, INFINITE);
    CloseHandle(sei.hProcess);
    QFile::remove(tempPath);
    return true;
#else
    Q_UNUSED(installDir)
    Q_UNUSED(package)
    return false;
#endif
}

// ── Registry lookup ─────────────────────────────────────────────────────────

QString InstallerEngine::findInstallLocation(const QString& appName)
{
    QSettings uninstall(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\"
                       "CurrentVersion\\Uninstall\\%1").arg(appName),
        QSettings::NativeFormat);
    return uninstall.value(QStringLiteral("InstallLocation")).toString();
}

// ── Accessors ────────────────────────────────────────────────────────────────

QString InstallerEngine::appName() const {
    return m_package.value(QStringLiteral("installer")).toObject()
                .value(QStringLiteral("name")).toString();
}
QString InstallerEngine::version() const {
    return m_package.value(QStringLiteral("installer")).toObject()
                .value(QStringLiteral("version")).toString();
}
QString InstallerEngine::publisher() const {
    return m_package.value(QStringLiteral("installer")).toObject()
                .value(QStringLiteral("publisher")).toString();
}
QString InstallerEngine::defaultInstallPath() const {
    const QJsonObject inst = m_package.value(QStringLiteral("installer")).toObject();
    return QDir(resolvePath(inst.value(QStringLiteral("default_install_root")).toString()))
        .absoluteFilePath(inst.value(QStringLiteral("default_install_folder")).toString());
}
QString InstallerEngine::mainExecutable() const {
    return m_package.value(QStringLiteral("installer")).toObject()
                .value(QStringLiteral("main_executable")).toString();
}
QString InstallerEngine::licenseText() const {
    const QString lf = m_package.value(QStringLiteral("license_file")).toString();
    if (lf.isEmpty()) return {};
    QFile f(QStringLiteral(":/package/files/%1").arg(lf));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return tr("(无法加载许可协议)");
    return QString::fromUtf8(f.readAll());
}
QString InstallerEngine::finishMessage() const {
    return m_package.value(QStringLiteral("finish_message")).toString();
}

QString InstallerEngine::startMenuFolder() const {
    QString s = m_package.value(QStringLiteral("installer")).toObject()
                    .value(QStringLiteral("start_menu_folder")).toString();
    return s.isEmpty() ? appName() : s;
}

QString InstallerEngine::welcomeText() const {
    return m_package.value(QStringLiteral("installer")).toObject()
                .value(QStringLiteral("welcome_text")).toString();
}

QString InstallerEngine::appIcon() const {
    return m_package.value(QStringLiteral("installer")).toObject()
                .value(QStringLiteral("app_icon")).toString();
}

QString InstallerEngine::supportUrl() const {
    return m_package.value(QStringLiteral("installer")).toObject()
                .value(QStringLiteral("support_url")).toString();
}

QString InstallerEngine::updateUrl() const {
    return m_package.value(QStringLiteral("installer")).toObject()
                .value(QStringLiteral("update_url")).toString();
}

bool InstallerEngine::requireAdmin() const {
    return m_package.value(QStringLiteral("installer")).toObject()
                .value(QStringLiteral("require_admin")).toBool(false);
}

QString InstallerEngine::description() const {
    return m_package.value(QStringLiteral("installer")).toObject()
                .value(QStringLiteral("description")).toString();
}

bool InstallerEngine::autoStart() const {
    return m_package.value(QStringLiteral("installer")).toObject()
                .value(QStringLiteral("auto_start")).toBool(false);
}

bool InstallerEngine::requireRestart() const {
    return m_package.value(QStringLiteral("installer")).toObject()
                .value(QStringLiteral("require_restart")).toBool(false);
}

bool InstallerEngine::runAfterInstall() const {
    return m_package.value(QStringLiteral("installer")).toObject()
                .value(QStringLiteral("run_after_install")).toBool(true);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  INSTALL
// ═══════════════════════════════════════════════════════════════════════════════

void InstallerEngine::install(const QString& installDir)
{
    int fileCount = 0;
    {
        QDirIterator it(QStringLiteral(":/package/files/"),
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) { it.next(); ++fileCount; }
    }
    const int totalSteps = fileCount + (autoStart() ? 5 : 4);
    int step = 0;

    emit statusChanged(tr("正在准备安装..."));
    emit progressChanged(0);

    copyFiles(installDir, step, totalSteps);

    step++;
    emit statusChanged(tr("正在写入注册表..."));
    emit progressChanged(step * 100 / totalSteps);
    writeRegistry(installDir);

    step++;
    emit statusChanged(tr("正在创建快捷方式..."));
    emit progressChanged(step * 100 / totalSteps);
    createShortcuts(installDir);

    step++;
    emit statusChanged(tr("正在部署卸载程序..."));
    emit progressChanged(step * 100 / totalSteps);
    deployUninstaller(installDir);

    if (autoStart()) {
        step++;
        emit statusChanged(tr("正在设置开机自启..."));
        emit progressChanged(step * 100 / totalSteps);
        QSettings runKey(
            QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\"
                           "CurrentVersion\\Run"),
            QSettings::NativeFormat);
        runKey.setValue(appName(),
            QDir::toNativeSeparators(
                QDir(installDir).absoluteFilePath(mainExecutable())));
        runKey.sync();
    }

    step++;
    emit statusChanged(tr("正在注册卸载信息..."));
    emit progressChanged(step * 100 / totalSteps);
    writeUninstallInfo(installDir);

    emit progressChanged(100);
    emit statusChanged(tr("安装完成！"));
    emit finished(true, QString());
}

void InstallerEngine::copyFiles(const QString& installDir, int& step, int totalSteps)
{
    QDir baseDir(QStringLiteral(":/package/files/"));
    QDirIterator it(QStringLiteral(":/package/files/"),
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString relative = baseDir.relativeFilePath(it.filePath());
        const QString target   = QDir(installDir).absoluteFilePath(relative);
        QDir().mkpath(QFileInfo(target).absolutePath());
        if (QFile::exists(target)) QFile::remove(target);
        if (!QFile::copy(it.filePath(), target)) {
            emit finished(false, tr("文件复制失败:\n%1\n→ %2").arg(it.filePath(), target));
            return;
        }
        step++;
        emit progressChanged(step * 100 / totalSteps);
        emit statusChanged(tr("正在复制: %1").arg(relative));
    }
}

void InstallerEngine::writeRegistry(const QString& installDir)
{
    const QJsonObject regObj = m_package.value(QStringLiteral("registry")).toObject();
    const QString ver = version();
    for (auto it = regObj.begin(); it != regObj.end(); ++it) {
        QSettings settings(it.key(), QSettings::NativeFormat);
        const QJsonObject vals = it.value().toObject();
        for (auto vi = vals.begin(); vi != vals.end(); ++vi) {
            QString rawVal = vi.value().toString();
            rawVal.replace(QStringLiteral("{VERSION}"), ver);
            rawVal.replace(QStringLiteral("{INSTALL_DIR}"), installDir);
            settings.setValue(vi.key(), rawVal);
        }
        settings.sync();
    }
}

void InstallerEngine::createShortcuts(const QString& installDir)
{
    const QJsonArray shortcuts = m_package.value(QStringLiteral("shortcuts")).toArray();
    const QString mainExe      = QDir(installDir).absoluteFilePath(mainExecutable());
    const QString name         = appName();
    const QString smFolder     = startMenuFolder();
    const QString iconFile     = appIcon();
    const QString iconPath     = iconFile.isEmpty() ? mainExe
                                : QDir(installDir).absoluteFilePath(iconFile);

    for (const QJsonValue& val : shortcuts) {
        const QJsonObject sc = val.toObject();
        const QString target = QDir(installDir).absoluteFilePath(
            sc.value(QStringLiteral("target")).toString());
        const QString location = sc.value(QStringLiteral("location")).toString();

        QString lnkPath;
        if (location.compare(QStringLiteral("Desktop"), Qt::CaseInsensitive) == 0) {
            lnkPath = Shortcut::desktopPath() + QStringLiteral("/%1.lnk").arg(name);
        } else if (location.compare(QStringLiteral("StartMenu"), Qt::CaseInsensitive) == 0) {
            lnkPath = Shortcut::programsMenuPath()
                      + QStringLiteral("/%1/%2.lnk").arg(smFolder, name);
        } else {
            continue;
        }

        Shortcut::create(target, lnkPath, name, iconPath, installDir);
    }
}

void InstallerEngine::writeUninstallInfo(const QString& installDir)
{
    const QString name = appName();
    QSettings uninstall(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\"
                       "CurrentVersion\\Uninstall\\%1").arg(name),
        QSettings::NativeFormat);
    uninstall.setValue(QStringLiteral("DisplayName"),     name);
    uninstall.setValue(QStringLiteral("DisplayVersion"),  version());
    uninstall.setValue(QStringLiteral("Publisher"),       publisher());
    uninstall.setValue(QStringLiteral("InstallLocation"), installDir);
    if (!description().isEmpty())
        uninstall.setValue(QStringLiteral("Comments"), description());
    uninstall.setValue(QStringLiteral("UninstallString"),
        QDir::toNativeSeparators(
            QDir(installDir).absoluteFilePath(QStringLiteral("Uninstall.exe"))));
    // Optional fields
    if (!supportUrl().isEmpty())
        uninstall.setValue(QStringLiteral("URLInfoAbout"), supportUrl());
    if (!updateUrl().isEmpty())
        uninstall.setValue(QStringLiteral("URLUpdateInfo"), updateUrl());
    if (!appIcon().isEmpty())
        uninstall.setValue(QStringLiteral("DisplayIcon"),
            QDir::toNativeSeparators(
                QDir(installDir).absoluteFilePath(appIcon())));
    uninstall.sync();
}

// ── Deploy uninstaller ─────────────────────────────────────────────────────

void InstallerEngine::deployUninstaller(const QString& installDir)
{
    const QString selfPath   = QCoreApplication::applicationFilePath();
    const QString targetPath = QDir(installDir).absoluteFilePath(
        QStringLiteral("Uninstall.exe"));

    if (QFile::exists(targetPath))
        QFile::remove(targetPath);
    QFile::copy(selfPath, targetPath);

    // Write uninstall.json so Uninstall.exe auto-detects its task
    QJsonObject config;
    config[QStringLiteral("app_name")]    = appName();
    config[QStringLiteral("install_dir")] = installDir;
    config[QStringLiteral("version")]     = version();

    QFile cf(QDir(installDir).absoluteFilePath(QStringLiteral("uninstall.json")));
    if (cf.open(QIODevice::WriteOnly)) {
        cf.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
        cf.close();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  UNINSTALL
// ═══════════════════════════════════════════════════════════════════════════════

void InstallerEngine::uninstall(const QString& installDir)
{
    // Count files to remove
    int fileCount = 0;
    {
        QDirIterator it(installDir, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) { it.next(); ++fileCount; }
    }
    const int totalSteps = fileCount + 3;
    int step = 0;

    emit statusChanged(tr("正在准备卸载..."));
    emit progressChanged(0);

    removeFiles(installDir, step, totalSteps);

    step++;
    emit statusChanged(tr("正在移除快捷方式..."));
    emit progressChanged(step * 100 / totalSteps);
    removeShortcuts(appName());

    step++;
    emit statusChanged(tr("正在清理注册表..."));
    emit progressChanged(step * 100 / totalSteps);
    removeRegistry(appName());

    // Try to remove the (now empty) install directory
    step++;
    emit statusChanged(tr("正在清理安装目录..."));
    emit progressChanged(step * 100 / totalSteps);
    QDir(installDir).removeRecursively();

    emit progressChanged(100);
    emit statusChanged(tr("卸载完成！"));
    emit finished(true, QString());
}

void InstallerEngine::removeFiles(const QString& installDir,
                                  int& step, int totalSteps)
{
    QDirIterator it(installDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (!QFile::remove(it.filePath())) {
            emit finished(false,
                tr("无法删除文件:\n%1").arg(it.filePath()));
            return;
        }
        step++;
        emit progressChanged(step * 100 / totalSteps);
        emit statusChanged(tr("正在删除: %1")
            .arg(QDir(installDir).relativeFilePath(it.filePath())));
    }
}

void InstallerEngine::removeShortcuts(const QString& appName)
{
    // Desktop
    QFile::remove(Shortcut::desktopPath()
                  + QStringLiteral("/%1.lnk").arg(appName));
    // Start Menu
    QFile::remove(Shortcut::programsMenuPath()
                  + QStringLiteral("/%1.lnk").arg(appName));
}

void InstallerEngine::removeRegistry(const QString& appName)
{
    // App registry key
    const QJsonObject regObj = m_package.value(QStringLiteral("registry")).toObject();
    for (auto it = regObj.begin(); it != regObj.end(); ++it) {
        QSettings settings(it.key(), QSettings::NativeFormat);
        settings.remove(QString());   // remove all values
        // Remove the key itself (if supported by QSettings)
        // QSettings doesn't support deleting keys; we leave an empty key.
    }

    // Uninstall registry key
    QSettings uninstall(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\"
                       "CurrentVersion\\Uninstall\\%1").arg(appName),
        QSettings::NativeFormat);
    uninstall.remove(QString());
}
