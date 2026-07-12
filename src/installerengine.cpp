#include "installerengine.hpp"
#include "shortcut.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryFile>

#ifdef Q_OS_WIN
#  include <windows.h>
#  include <shellapi.h>
#endif

// ═══════════════════════════════════════════════════════════════════════════════
InstallerEngine::InstallerEngine(QObject* parent) : QObject(parent) {}

// ═══════════════════════════════════════════════════════════════════════════════
//  Package
// ═══════════════════════════════════════════════════════════════════════════════

bool InstallerEngine::loadPackage()
{
    QFile f(":/package/package.json");
    if (!f.open(QIODevice::ReadOnly)) {
        emit finished(false, tr("无法读取内嵌的 package.json"));
        return false;
    }
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (doc.isNull()) {
        emit finished(false, tr("package.json 解析失败"));
        return false;
    }
    m_package = doc.object();
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Static helpers
// ═══════════════════════════════════════════════════════════════════════════════

QString InstallerEngine::resolvePath(const QString& raw)
{
    QString s = raw;
    s.replace(QStringLiteral("$LOCALAPPDATA$"),
              QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
    s.replace(QStringLiteral("$APPDATA$"),
              QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    s.replace(QStringLiteral("$PROGRAMFILES$"),
              QDir::fromNativeSeparators(qEnvironmentVariable("ProgramFiles")));
    s.replace(QStringLiteral("$PROGRAMFILES64$"),
              QDir::fromNativeSeparators(qEnvironmentVariable("ProgramFiles")));
    s.replace(QStringLiteral("$PROGRAMFILES32$"),
              QDir::fromNativeSeparators(qEnvironmentVariable("ProgramFiles(x86)")));
    s.replace(QStringLiteral("$DESKTOP$"),
              QStandardPaths::writableLocation(QStandardPaths::DesktopLocation));
    s.replace(QStringLiteral("$STARTMENU$"),
              QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation));
    return QDir::toNativeSeparators(s);
}

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

    const QString exe = QCoreApplication::applicationFilePath();
    const QString params = QStringLiteral("--install-silent \"%1\"").arg(tempPath);
    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);  sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = reinterpret_cast<LPCWSTR>(exe.utf16());
    sei.lpParameters = reinterpret_cast<LPCWSTR>(params.utf16());
    sei.nShow = SW_NORMAL;
    if (!ShellExecuteExW(&sei)) return false;
    WaitForSingleObject(sei.hProcess, INFINITE);
    CloseHandle(sei.hProcess);
    QFile::remove(tempPath);
    return true;
#else
    Q_UNUSED(installDir); Q_UNUSED(package); return false;
#endif
}

QString InstallerEngine::findInstallLocation(const QString& appName)
{
    QSettings uninstall(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\"
                       "CurrentVersion\\Uninstall\\%1").arg(appName),
        QSettings::NativeFormat);
    return uninstall.value(QStringLiteral("InstallLocation")).toString();
}

bool InstallerEngine::checkOsVersion(const QString& minVer)
{
#ifdef Q_OS_WIN
    if (minVer.isEmpty()) return true;
    // Parse minVer: "10.0.17763"
    QStringList parts = minVer.split('.');
    if (parts.size() < 3) return true;
    DWORD reqMajor = parts[0].toUInt();
    DWORD reqMinor = parts[1].toUInt();
    DWORD reqBuild = parts[2].toUInt();

    // Use RtlGetVersion for accurate version
    typedef LONG (WINAPI *RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return true;
    auto RtlGetVersion = reinterpret_cast<RtlGetVersionPtr>(
        GetProcAddress(ntdll, "RtlGetVersion"));
    if (!RtlGetVersion) return true;

    RTL_OSVERSIONINFOW osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if (RtlGetVersion(&osvi) != 0) return true;

    if (osvi.dwMajorVersion < reqMajor) return false;
    if (osvi.dwMajorVersion > reqMajor) return true;
    if (osvi.dwMinorVersion < reqMinor) return false;
    if (osvi.dwMinorVersion > reqMinor) return true;
    return osvi.dwBuildNumber >= reqBuild;
#else
    Q_UNUSED(minVer); return true;
#endif
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Accessors — installer.*
// ═══════════════════════════════════════════════════════════════════════════════

#define I_OBJ m_package.value(QStringLiteral("installer")).toObject()
#define I_STR(k) I_OBJ.value(QStringLiteral(k)).toString()
#define I_BOOL(k,d) I_OBJ.value(QStringLiteral(k)).toBool(d)
#define U_OBJ m_package.value(QStringLiteral("uninstall")).toObject()

QString InstallerEngine::appName() const    { return I_STR("name"); }
QString InstallerEngine::version() const     { return I_STR("version"); }
QString InstallerEngine::publisher() const   { return I_STR("publisher"); }
QString InstallerEngine::copyright() const   { return I_STR("copyright"); }
QString InstallerEngine::website() const     { return I_STR("website"); }
QString InstallerEngine::iconPath() const    { return I_STR("icon"); }
QString InstallerEngine::bannerImage() const { return I_STR("banner_image"); }
bool    InstallerEngine::requiresAdmin() const   { return I_BOOL("requires_admin", false); }
bool    InstallerEngine::allowChangePath() const { return I_BOOL("allow_change_path", true); }

QString InstallerEngine::shortName() const
{
    QString s = I_STR("short_name");
    if (!s.isEmpty()) return s;
    s = appName();
    s.replace(QRegularExpression(QStringLiteral("[^a-zA-Z0-9_]")), QStringLiteral("_"));
    return s.toLower();
}

QString InstallerEngine::defaultInstallPath() const
{
    QString root = resolvePath(I_STR("default_install_root"));
    QString folder = I_STR("default_install_folder");
    return QDir(root).absoluteFilePath(folder);
}

QString InstallerEngine::mainExecutable() const { return I_STR("main_executable"); }

QString InstallerEngine::licenseText() const
{
    QString lf = I_STR("license_file");
    if (lf.isEmpty()) return {};
    QFile f(QStringLiteral(":/package/files/%1").arg(lf));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return tr("(无法加载许可协议)");
    return QString::fromUtf8(f.readAll());
}

QString InstallerEngine::welcomeTitle() const
{
    QString t = I_STR("welcome_title");
    if (t.isEmpty()) t = tr("欢迎使用 %1 安装向导").arg(appName());
    t.replace(QStringLiteral("{name}"), appName());
    t.replace(QStringLiteral("{version}"), version());
    return t;
}

QString InstallerEngine::welcomeText() const
{
    QString t = I_STR("welcome_text");
    if (t.isEmpty())
        t = tr("<p>本向导将引导您完成 <b>%1</b> 的安装。</p>").arg(appName());
    t.replace(QStringLiteral("{name}"), appName());
    t.replace(QStringLiteral("{publisher}"), publisher());
    t.replace(QStringLiteral("{version}"), version());
    return t;
}

QString InstallerEngine::finishTitle() const
{
    QString t = I_STR("finish_title");
    return t.isEmpty() ? tr("安装完成") : t;
}

QString InstallerEngine::finishMessage() const
{
    QString t = m_package.value(QStringLiteral("finish_message")).toString();
    if (t.isEmpty()) t = tr("%1 已成功安装。").arg(appName());
    t.replace(QStringLiteral("{name}"), appName());
    return t;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Accessors — files_strategy.*
// ═══════════════════════════════════════════════════════════════════════════════

QString InstallerEngine::fileMode() const
{
    return m_package.value(QStringLiteral("files_strategy")).toObject()
                .value(QStringLiteral("mode")).toString(QStringLiteral("mirror"));
}

QString InstallerEngine::fileOverwrite() const
{
    return m_package.value(QStringLiteral("files_strategy")).toObject()
                .value(QStringLiteral("overwrite")).toString(QStringLiteral("ask"));
}

QStringList InstallerEngine::fileExclude() const
{
    QStringList list;
    const QJsonArray arr = m_package.value(QStringLiteral("files_strategy")).toObject()
                               .value(QStringLiteral("exclude")).toArray();
    for (const auto& v : arr) list.append(v.toString());
    return list;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Accessors — uninstall.*
// ═══════════════════════════════════════════════════════════════════════════════

QString InstallerEngine::uninstallString() const
{
    return U_OBJ.value(QStringLiteral("uninstall_string")).toString();
}

QString InstallerEngine::displayIcon() const
{
    QString s = U_OBJ.value(QStringLiteral("display_icon")).toString();
    if (s.isEmpty()) s = I_STR("uninstall_icon");
    return s;
}

bool InstallerEngine::uninstallNoModify() const
{
    return U_OBJ.value(QStringLiteral("no_modify")).toBool(true);
}

bool InstallerEngine::uninstallNoRepair() const
{
    return U_OBJ.value(QStringLiteral("no_repair")).toBool(true);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Accessors — finish_actions.*
// ═══════════════════════════════════════════════════════════════════════════════

#define F_OBJ m_package.value(QStringLiteral("finish_actions")).toObject()

bool    InstallerEngine::finishRunProgram() const
{ return F_OBJ.value(QStringLiteral("run_program")).toBool(true); }
QString InstallerEngine::finishRunArgs() const
{ return F_OBJ.value(QStringLiteral("run_arguments")).toString(); }
QString InstallerEngine::finishOpenReadme() const
{ return F_OBJ.value(QStringLiteral("open_readme")).toString(); }
bool    InstallerEngine::finishRestartRequired() const
{ return F_OBJ.value(QStringLiteral("restart_required")).toBool(false); }

// ═══════════════════════════════════════════════════════════════════════════════
//  Accessors — advanced.*
// ═══════════════════════════════════════════════════════════════════════════════

#define A_OBJ m_package.value(QStringLiteral("advanced")).toObject()

bool    InstallerEngine::advancedCreateUninstaller() const
{ return A_OBJ.value(QStringLiteral("create_uninstaller")).toBool(true); }
bool    InstallerEngine::advancedEnableLogging() const
{ return A_OBJ.value(QStringLiteral("enable_logging")).toBool(false); }
bool    InstallerEngine::advancedSilentInstall() const
{ return A_OBJ.value(QStringLiteral("silent_install")).toBool(false); }
QString InstallerEngine::advancedMinOsVersion() const
{ return A_OBJ.value(QStringLiteral("minimum_os_version")).toString(); }
bool    InstallerEngine::advanced64BitOnly() const
{ return A_OBJ.value(QStringLiteral("64_bit_only")).toBool(false); }

QStringList InstallerEngine::advancedUpdatePreservePatterns() const
{
    QStringList list;
    const QJsonArray arr = A_OBJ.value(QStringLiteral("update_preserve_patterns")).toArray();
    for (const auto& v : arr) list.append(v.toString());
    // Default patterns if none specified
    if (list.isEmpty()) list << QStringLiteral("*.ini") << QStringLiteral("*.cfg")
                              << QStringLiteral("*.conf") << QStringLiteral("config/*")
                              << QStringLiteral("data/*")  << QStringLiteral("*.db");
    return list;
}

// ── Installation detection ──────────────────────────────────────────────────

InstallerEngine::InstalledInfo
InstallerEngine::getInstalledInfo(const QString& appName)
{
    InstalledInfo info;
    QSettings uk(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\"
                       "CurrentVersion\\Uninstall\\%1").arg(appName),
        QSettings::NativeFormat);
    QString path = uk.value(QStringLiteral("InstallLocation")).toString();
    if (path.isEmpty()) {
        // Try with short_name variant
        QSettings uk2(
            QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\"
                           "CurrentVersion\\Uninstall\\%1")
                .arg(appName.toLower().replace(QRegularExpression(QStringLiteral("[^a-z0-9_]")),
                                               QStringLiteral("_"))),
            QSettings::NativeFormat);
        path = uk2.value(QStringLiteral("InstallLocation")).toString();
    }
    if (!path.isEmpty() && QDir(path).exists()) {
        info.installed   = true;
        info.installPath = path;
        info.version     = uk.value(QStringLiteral("DisplayVersion")).toString();
    }
    return info;
}

// ── Silent update (auto-update) ─────────────────────────────────────────────

void InstallerEngine::silentUpdate(const QString& installDir)
{
    int fileCount = 0;
    {
        QDirIterator it(QStringLiteral(":/package/files/"),
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) { it.next(); ++fileCount; }
    }

    const QStringList preservePatterns = advancedUpdatePreservePatterns();
    const int totalSteps = fileCount + 3;  // backup + copy + cleanup
    int step = 0;

    emit statusChanged(tr("正在准备更新..."));
    emit progressChanged(0);

    // ── 1. Backup preserved files ─────────────────────────────────────────
    QMap<QString, QByteArray> preservedData;
    if (QDir(installDir).exists()) {
        for (const QString& pat : preservePatterns) {
            QRegularExpression re(QRegularExpression::wildcardToRegularExpression(pat),
                                  QRegularExpression::CaseInsensitiveOption);
            QDirIterator it(installDir, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                QString rel = QDir(installDir).relativeFilePath(it.filePath());
                if (re.match(rel).hasMatch()) {
                    QFile f(it.filePath());
                    if (f.open(QIODevice::ReadOnly))
                        preservedData[rel] = f.readAll();
                }
            }
        }
    }
    step++;
    emit progressChanged(step * 100 / totalSteps);
    emit statusChanged(tr("正在备份配置文件..."));

    // ── 2. Remove old files (keep directory structure for config though) ──
    {
        QDirIterator it(installDir, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) { it.next(); QFile::remove(it.filePath()); }
    }

    // ── 3. Copy new files ─────────────────────────────────────────────────
    {
        QDir baseDir(QStringLiteral(":/package/files/"));
        QDirIterator it(QStringLiteral(":/package/files/"),
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            const QString rel    = baseDir.relativeFilePath(it.filePath());
            const QString target = QDir(installDir).absoluteFilePath(rel);
            QDir().mkpath(QFileInfo(target).absolutePath());
            if (QFile::exists(target)) QFile::remove(target);
            if (!QFile::copy(it.filePath(), target)) {
                emit finished(false, tr("文件复制失败:\n%1").arg(rel));
                return;
            }
            step++;
            emit progressChanged(step * 100 / totalSteps);
            emit statusChanged(tr("正在更新: %1").arg(rel));
        }
    }

    // ── 4. Restore preserved files ────────────────────────────────────────
    for (auto it = preservedData.begin(); it != preservedData.end(); ++it) {
        QString target = QDir(installDir).absoluteFilePath(it.key());
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile f(target);
        if (f.open(QIODevice::WriteOnly)) {
            f.write(it.value());
            f.close();
        }
    }

    // ── 5. Update registry version ────────────────────────────────────────
    writeRegistry(installDir);
    writeUninstallInfo(installDir);

    emit progressChanged(100);
    emit statusChanged(tr("更新完成！"));
    emit finished(true, {});
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

    const QJsonArray sc = m_package.value(QStringLiteral("shortcuts")).toArray();
    const QJsonObject env = m_package.value(QStringLiteral("environment")).toObject();
    const QJsonArray fa  = m_package.value(QStringLiteral("file_associations")).toArray();

    int extraSteps = 0;
    if (!sc.isEmpty()) extraSteps++;
    if (!env.isEmpty()) extraSteps++;
    if (!fa.isEmpty()) extraSteps++;
    if (advancedCreateUninstaller()) extraSteps++;
    extraSteps++; // uninstall info always

    const int totalSteps = fileCount + extraSteps;
    int step = 0;

    emit statusChanged(tr("正在准备安装..."));
    emit progressChanged(0);

    copyFiles(installDir, step, totalSteps);

    step++; emit statusChanged(tr("正在写入注册表...")); emit progressChanged(step * 100 / totalSteps);
    writeRegistry(installDir);

    if (!sc.isEmpty()) {
        step++; emit statusChanged(tr("正在创建快捷方式...")); emit progressChanged(step * 100 / totalSteps);
        createShortcuts(installDir);
    }

    if (!env.isEmpty()) {
        step++; emit statusChanged(tr("正在设置环境变量...")); emit progressChanged(step * 100 / totalSteps);
        writeEnvironment(installDir);
    }

    if (!fa.isEmpty()) {
        step++; emit statusChanged(tr("正在注册文件关联...")); emit progressChanged(step * 100 / totalSteps);
        writeFileAssociations(installDir);
    }

    if (advancedCreateUninstaller()) {
        step++; emit statusChanged(tr("正在部署卸载程序...")); emit progressChanged(step * 100 / totalSteps);
        deployUninstaller(installDir);
    }

    step++; emit statusChanged(tr("正在注册卸载信息...")); emit progressChanged(step * 100 / totalSteps);
    writeUninstallInfo(installDir);

    emit progressChanged(100);
    emit statusChanged(tr("安装完成！"));
    emit finished(true, {});
}

// ── File copy ────────────────────────────────────────────────────────────────

void InstallerEngine::copyFiles(const QString& installDir, int& step, int totalSteps)
{
    QDir baseDir(QStringLiteral(":/package/files/"));
    QStringList excludes = fileExclude();

    QDirIterator it(QStringLiteral(":/package/files/"),
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString rel = baseDir.relativeFilePath(it.filePath());

        // Check exclude patterns (simple glob: *.ext or dir/**)
        bool skip = false;
        for (const QString& pat : excludes) {
            QRegularExpression re(QRegularExpression::wildcardToRegularExpression(pat));
            if (re.match(rel).hasMatch()) { skip = true; break; }
        }
        if (skip) continue;

        QString target = QDir(installDir).absoluteFilePath(rel);
        QDir().mkpath(QFileInfo(target).absolutePath());
        if (QFile::exists(target)) QFile::remove(target);
        if (!QFile::copy(it.filePath(), target)) {
            emit finished(false, tr("文件复制失败:\n%1").arg(rel));
            return;
        }
        step++;
        emit progressChanged(step * 100 / totalSteps);
        emit statusChanged(tr("正在复制: %1").arg(rel));
    }
}

// ── Registry ─────────────────────────────────────────────────────────────────

void InstallerEngine::writeRegistry(const QString& installDir)
{
    const QJsonObject regObj = m_package.value(QStringLiteral("registry")).toObject();
    for (auto it = regObj.begin(); it != regObj.end(); ++it) {
        QSettings settings(it.key(), QSettings::NativeFormat);
        const QJsonObject vals = it.value().toObject();
        for (auto vi = vals.begin(); vi != vals.end(); ++vi) {
            QString v = vi.value().toString();
            v.replace(QStringLiteral("{VERSION}"), version());
            v.replace(QStringLiteral("{INSTALL_DIR}"), installDir);
            settings.setValue(vi.key(), v);
        }
        settings.sync();
    }
}

// ── Shortcuts ────────────────────────────────────────────────────────────────

void InstallerEngine::createShortcuts(const QString& installDir)
{
    const QJsonArray scs = m_package.value(QStringLiteral("shortcuts")).toArray();
    for (const QJsonValue& v : scs) {
        QJsonObject s = v.toObject();
        QString target = QDir(installDir).absoluteFilePath(
            s.value(QStringLiteral("target")).toString());
        QString loc = s.value(QStringLiteral("location")).toString();
        QString name = s.value(QStringLiteral("name")).toString();
        if (name.isEmpty()) name = QFileInfo(target).baseName();
        QString desc = s.value(QStringLiteral("description")).toString();
        QString args = s.value(QStringLiteral("arguments")).toString();
        QString wd   = s.value(QStringLiteral("working_dir")).toString();
        QString icon = s.value(QStringLiteral("icon")).toString();
        int iconIdx = 0;
        if (icon.contains(',')) {
            iconIdx = icon.section(',', 1).toInt();
            icon = icon.section(',', 0, 0);
        }
        if (!icon.isEmpty() && !QDir::isAbsolutePath(icon))
            icon = QDir(installDir).absoluteFilePath(icon);
        if (icon.isEmpty()) icon = target;
        QString wdAbs = wd.isEmpty() ? QFileInfo(target).absolutePath()
                        : QDir(installDir).absoluteFilePath(wd);
        bool runAsAdmin = s.value(QStringLiteral("run_as_admin")).toBool(false);

        QString lnkPath;
        QString smFolder = s.value(QStringLiteral("start_menu_folder")).toString();
        if (loc == QStringLiteral("Desktop"))
            lnkPath = Shortcut::desktopPath() + "/" + name + ".lnk";
        else if (loc == QStringLiteral("StartMenu"))
            lnkPath = Shortcut::programsMenuPath() + "/"
                      + (smFolder.isEmpty() ? "" : smFolder + "/") + name + ".lnk";
        else if (loc == QStringLiteral("Startup"))
            lnkPath = Shortcut::startupPath() + "/" + name + ".lnk";
        else continue;

        Shortcut::create(target, lnkPath, desc, args, icon, iconIdx, wdAbs, runAsAdmin);
    }
}

// ── Environment variables ────────────────────────────────────────────────────

void InstallerEngine::writeEnvironment(const QString& installDir)
{
    const QJsonObject env = m_package.value(QStringLiteral("environment")).toObject();
    for (const QString& scope : {QStringLiteral("user"), QStringLiteral("system")}) {
        QJsonObject vars = env.value(scope).toObject();
        if (vars.isEmpty()) continue;

        QString regPath = (scope == QStringLiteral("user"))
            ? QStringLiteral("HKEY_CURRENT_USER\\Environment")
            : QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\"
                             "Control\\Session Manager\\Environment");

        for (auto it = vars.begin(); it != vars.end(); ++it) {
            QString val = it.value().toString();
            val.replace(QStringLiteral("{INSTALL_DIR}"), installDir);

            if (it.key() == QStringLiteral("PATH")) {
                // Append to existing PATH
                QSettings s(regPath, QSettings::NativeFormat);
                QString existing = s.value(QStringLiteral("PATH")).toString();
                if (!existing.isEmpty()) {
                    if (!existing.endsWith(';')) existing += ';';
                    val = existing + val;
                }
            }

            QSettings s(regPath, QSettings::NativeFormat);
            s.setValue(it.key(), val);
            s.sync();
        }
    }

    // Broadcast environment change
#ifdef Q_OS_WIN
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0,
        reinterpret_cast<LPARAM>(L"Environment"),
        SMTO_ABORTIFHUNG, 5000, nullptr);
#endif
}

// ── File associations ────────────────────────────────────────────────────────

void InstallerEngine::writeFileAssociations(const QString& installDir)
{
    const QJsonArray fas = m_package.value(QStringLiteral("file_associations")).toArray();
    for (const QJsonValue& v : fas) {
        QJsonObject fa = v.toObject();
        QString ext     = fa.value(QStringLiteral("extension")).toString();
        QString desc    = fa.value(QStringLiteral("description")).toString();
        QString openCmd = fa.value(QStringLiteral("open_command")).toString();
        openCmd.replace(QStringLiteral("{INSTALL_DIR}"), installDir);
        QString progId = fa.value(QStringLiteral("prog_id")).toString();
        if (progId.isEmpty())
            progId = publisher() + "." + shortName() + ext;

        // HKCU\Software\Classes\.ext
        QSettings extKey(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\%1").arg(ext),
                         QSettings::NativeFormat);
        extKey.setValue(QStringLiteral(""), progId);

        // HKCU\Software\Classes\ProgId
        QSettings progKey(
            QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\%1").arg(progId),
            QSettings::NativeFormat);
        progKey.setValue(QStringLiteral(""), desc);

        // Icon
        QString icon = fa.value(QStringLiteral("icon")).toString();
        if (!icon.isEmpty()) {
            QSettings iconKey(progKey.fileName() + QStringLiteral("\\DefaultIcon"),
                              QSettings::NativeFormat);
            iconKey.setValue(QStringLiteral(""), icon);
        }

        // Open command
        QSettings cmdKey(progKey.fileName() + QStringLiteral("\\shell\\open\\command"),
                         QSettings::NativeFormat);
        cmdKey.setValue(QStringLiteral(""), openCmd);

        extKey.sync(); progKey.sync();
    }
}

// ── Deploy uninstaller ──────────────────────────────────────────────────────

void InstallerEngine::deployUninstaller(const QString& installDir)
{
    const QString selfPath   = QCoreApplication::applicationFilePath();
    const QString targetPath = QDir(installDir).absoluteFilePath(
        QStringLiteral("Uninstall.exe"));

    // Remove old uninstaller if present
    if (QFile::exists(targetPath))
        QFile::remove(targetPath);

    // Use Windows CopyFileW — handles running exe correctly (CoW on NTFS)
    bool copied = false;
#ifdef Q_OS_WIN
    copied = CopyFileW(reinterpret_cast<LPCWSTR>(selfPath.utf16()),
                       reinterpret_cast<LPCWSTR>(targetPath.utf16()), FALSE);
#else
    copied = QFile::copy(selfPath, targetPath);
#endif

    if (!copied) {
        // Fallback: read + write manually
        QFile src(selfPath);
        if (src.open(QIODevice::ReadOnly)) {
            QFile dst(targetPath);
            if (dst.open(QIODevice::WriteOnly)) {
                dst.write(src.readAll());
                dst.close();
                copied = true;
            }
            src.close();
        }
    }

    // Write uninstall.json always (even if copy failed, the registry entry
    // pointing to it is still useful for manual fix)
    QJsonObject config;
    config[QStringLiteral("app_name")]    = appName();
    config[QStringLiteral("install_dir")] = installDir;
    config[QStringLiteral("version")]     = version();

    QFile cf(QDir(installDir).absoluteFilePath(QStringLiteral("uninstall.json")));
    if (cf.open(QIODevice::WriteOnly)) {
        cf.write(QJsonDocument(config).toJson(QJsonDocument::Indented));
        cf.close();
    }

    if (!copied)
        emit statusChanged(tr("警告：无法复制卸载程序到安装目录"));
}

// ── Uninstall info ───────────────────────────────────────────────────────────

void InstallerEngine::writeUninstallInfo(const QString& installDir)
{
    QString name = appName();
    QString sn   = shortName();

    // Use config key name or fallback to short_name
    QSettings u(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\"
                       "CurrentVersion\\Uninstall\\%1").arg(sn),
        QSettings::NativeFormat);

    u.setValue(QStringLiteral("DisplayName"),     name);
    u.setValue(QStringLiteral("DisplayVersion"),  version());
    u.setValue(QStringLiteral("Publisher"),       publisher());
    u.setValue(QStringLiteral("InstallLocation"), installDir);

    // Uninstall string
    QString us = uninstallString();
    if (us.isEmpty())
        us = QStringLiteral("\"%1\"").arg(
            QDir::toNativeSeparators(QDir(installDir).absoluteFilePath(QStringLiteral("Uninstall.exe"))));
    else
        us.replace(QStringLiteral("{INSTALL_DIR}"), installDir);
    u.setValue(QStringLiteral("UninstallString"), us);

    // Quiet uninstall
    QString qs = U_OBJ.value(QStringLiteral("quiet_uninstall_string")).toString();
    if (!qs.isEmpty()) {
        qs.replace(QStringLiteral("{INSTALL_DIR}"), installDir);
        u.setValue(QStringLiteral("QuietUninstallString"), qs);
    }

    // Icon
    QString di = displayIcon();
    if (!di.isEmpty()) {
        di.replace(QStringLiteral("{INSTALL_DIR}"), installDir);
        u.setValue(QStringLiteral("DisplayIcon"), QDir::toNativeSeparators(di));
    }

    // Estimated size
    int sizeKb = U_OBJ.value(QStringLiteral("estimated_size")).toInt(0);
    if (sizeKb == 0) {
        // auto-calculate
        qint64 total = 0;
        QDirIterator it(installDir, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) { it.next(); total += it.fileInfo().size(); }
        sizeKb = static_cast<int>(total / 1024);
    }
    if (sizeKb > 0) u.setValue(QStringLiteral("EstimatedSize"), sizeKb * 1024);

    u.setValue(QStringLiteral("NoModify"), uninstallNoModify() ? 1 : 0);
    u.setValue(QStringLiteral("NoRepair"), uninstallNoRepair() ? 1 : 0);

    if (!website().isEmpty())
        u.setValue(QStringLiteral("URLInfoAbout"), website());

    QString helpLink = U_OBJ.value(QStringLiteral("help_link")).toString();
    if (!helpLink.isEmpty())
        u.setValue(QStringLiteral("HelpLink"), helpLink);

    u.sync();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  UNINSTALL
// ═══════════════════════════════════════════════════════════════════════════════

void InstallerEngine::uninstall(const QString& installDir)
{
    int fileCount = 0;
    { QDirIterator it(installDir, QDir::Files, QDirIterator::Subdirectories);
      while (it.hasNext()) { it.next(); ++fileCount; } }

    const int totalSteps = fileCount + 5;
    int step = 0;
    emit statusChanged(tr("正在准备卸载...")); emit progressChanged(0);

    removeFiles(installDir, step, totalSteps);

    step++; emit statusChanged(tr("正在移除快捷方式...")); emit progressChanged(step * 100 / totalSteps);
    removeShortcuts(appName()); removeAutoStart(appName());

    step++; emit statusChanged(tr("正在清理注册表...")); emit progressChanged(step * 100 / totalSteps);
    removeRegistry(appName()); removeEnvironment(appName()); removeFileAssociations();

    step++; emit statusChanged(tr("正在清理安装目录...")); emit progressChanged(step * 100 / totalSteps);
    // Remove empty directories bottom-up
    QDir(installDir).removeRecursively();

    emit progressChanged(100); emit statusChanged(tr("卸载完成！"));
    emit finished(true, {});
}

void InstallerEngine::removeFiles(const QString& installDir, int& step, int totalSteps)
{
    // Files to skip (self-deleting): Uninstall.exe, uninstall.json
    const QString selfExe    = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
    const QString selfExeLower = selfExe.toLower();

    QStringList delayedDelete;
    int skipped = 0;

    QDirIterator it(installDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        QString fp = QDir::toNativeSeparators(it.filePath());

        // Skip the running uninstaller and its config
        if (fp.toLower() == selfExeLower) { skipped++; continue; }
        if (fp.endsWith(QStringLiteral("uninstall.json"), Qt::CaseInsensitive)) { skipped++; continue; }

        if (!QFile::remove(fp)) {
            // Schedule for reboot cleanup
#ifdef Q_OS_WIN
            MoveFileExW(reinterpret_cast<LPCWSTR>(fp.utf16()), nullptr,
                        MOVEFILE_DELAY_UNTIL_REBOOT);
            delayedDelete.append(fp);
#else
            emit finished(false, tr("无法删除文件:\n%1").arg(fp));
            return;
#endif
        }
        step++;
        emit progressChanged(step * 100 / totalSteps);
        emit statusChanged(tr("正在删除: %1").arg(QDir(installDir).relativeFilePath(fp)));
    }

    // Schedule self-deletion (Uninstall.exe) on reboot
#ifdef Q_OS_WIN
    MoveFileExW(reinterpret_cast<LPCWSTR>(selfExe.utf16()), nullptr,
                MOVEFILE_DELAY_UNTIL_REBOOT);
#endif

    if (!delayedDelete.isEmpty())
        emit statusChanged(tr("注意：%1 个文件将在重启后删除").arg(delayedDelete.size()));
}

void InstallerEngine::removeShortcuts(const QString& appName)
{
    QFile::remove(Shortcut::desktopPath() + "/" + appName + ".lnk");
    QFile::remove(Shortcut::startupPath() + "/" + appName + ".lnk");
    // StartMenu: try to find and remove
    QDir sm(Shortcut::programsMenuPath());
    for (const auto& entry : sm.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile::remove(entry.absoluteFilePath() + "/" + appName + ".lnk");
    }
    QFile::remove(sm.absoluteFilePath(appName + ".lnk"));
}

void InstallerEngine::removeRegistry(const QString& appName)
{
    QSettings appKey(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\%1").arg(appName),
        QSettings::NativeFormat);
    appKey.remove({});

    // Uninstall key — use both possible names
    QSettings uk1(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\"
                       "CurrentVersion\\Uninstall\\%1").arg(appName),
        QSettings::NativeFormat);
    uk1.remove({});

    QSettings uk2(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\"
                       "CurrentVersion\\Uninstall\\%1").arg(shortName()),
        QSettings::NativeFormat);
    uk2.remove({});

    // Extra registry
    const QJsonObject regObj = m_package.value(QStringLiteral("registry")).toObject();
    for (auto it = regObj.begin(); it != regObj.end(); ++it) {
        QSettings s(it.key(), QSettings::NativeFormat);
        s.remove({});
    }
}

void InstallerEngine::removeEnvironment(const QString& appName)
{
    const QJsonObject env = m_package.value(QStringLiteral("environment")).toObject();
    for (const QString& scope : {QStringLiteral("user"), QStringLiteral("system")}) {
        QJsonObject vars = env.value(scope).toObject();
        if (vars.isEmpty()) continue;
        QString regPath = (scope == QStringLiteral("user"))
            ? QStringLiteral("HKEY_CURRENT_USER\\Environment")
            : QStringLiteral("HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\"
                             "Control\\Session Manager\\Environment");
        for (auto it = vars.begin(); it != vars.end(); ++it) {
            if (it.key() == QStringLiteral("PATH")) {
                // Remove only the appended portion
                QString valToRemove = it.value().toString();
                QSettings s(regPath, QSettings::NativeFormat);
                QString existing = s.value(QStringLiteral("PATH")).toString();
                existing.replace(valToRemove + QChar(';'), QString());
                existing.replace(QChar(';') + valToRemove, QString());
                existing.replace(valToRemove, QString());
                s.setValue(QStringLiteral("PATH"), existing);
                s.sync();
            } else {
                QSettings s(regPath, QSettings::NativeFormat);
                s.remove(it.key());
            }
        }
    }
}

void InstallerEngine::removeFileAssociations()
{
    const QJsonArray fas = m_package.value(QStringLiteral("file_associations")).toArray();
    for (const QJsonValue& v : fas) {
        QJsonObject fa = v.toObject();
        QString ext = fa.value(QStringLiteral("extension")).toString();
        QString progId = fa.value(QStringLiteral("prog_id")).toString();
        if (progId.isEmpty())
            progId = publisher() + "." + shortName() + ext;

        QSettings extKey(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\%1").arg(ext),
                         QSettings::NativeFormat);
        extKey.remove({});
        QSettings progKey(QStringLiteral("HKEY_CURRENT_USER\\Software\\Classes\\%1").arg(progId),
                          QSettings::NativeFormat);
        progKey.remove({});
        // QSettings::remove("") removes all values but not subkeys,
        // but since we know the structure, this is sufficient for our cases.
    }
}

void InstallerEngine::removeAutoStart(const QString& appName)
{
    QSettings runKey(
        QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\"
                       "CurrentVersion\\Run"),
        QSettings::NativeFormat);
    runKey.remove(appName);
}
