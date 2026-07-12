#include "installerengine.hpp"
#include "installwizard.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>

static QJsonObject readJson(const QString& path)
{
    QFile f(path); if (!f.open(QIODevice::ReadOnly)) return {};
    auto doc = QJsonDocument::fromJson(f.readAll()); f.close();
    return doc.object();
}

// ── Silent install (elevated) ────────────────────────────────────────────────

static int runSilentInstall(const QString& stateFile)
{
    QJsonObject state = readJson(stateFile);
    if (state.isEmpty()) {
        QMessageBox::critical(nullptr, QStringLiteral("错误"), QStringLiteral("无法读取状态文件"));
        return 1;
    }
    QString dir = state[QStringLiteral("install_dir")].toString();
    InstallerEngine e; if (!e.loadPackage()) return 1;
    e.install(dir);
    return 0;
}

// ── Silent install (user-requested /S) ───────────────────────────────────────

static int runSilentInstall()
{
    InstallerEngine e; if (!e.loadPackage()) return 1;
    e.install(e.defaultInstallPath());
    return 0;
}

// ── Silent update (software-triggered auto-update) ───────────────────────────

static int runSilentUpdate(const QString& installDir)
{
    InstallerEngine e; if (!e.loadPackage()) return 1;
    e.silentUpdate(installDir);
    return 0;
}

// ── Uninstall wizard ─────────────────────────────────────────────────────────

static int runUninstallWizard(const QString& dir)
{
    InstallerEngine e; if (!e.loadPackage()) return 1;
    InstallWizard w(InstallWizard::Uninstall, e.package(), dir);
    w.exec();
    return 0;
}

// ── Auto-detect uninstall.json ───────────────────────────────────────────────

static bool tryAutoUninstall()
{
    QString cfg = QCoreApplication::applicationDirPath() + QStringLiteral("/uninstall.json");
    QJsonObject c = readJson(cfg);
    if (c.isEmpty()) return false;
    QString dir = c[QStringLiteral("install_dir")].toString();
    if (dir.isEmpty()) return false;
    runUninstallWizard(dir);
    return true;
}

// ── Normal install (with upgrade detection) ──────────────────────────────────

static int runInstallWizard()
{
    InstallerEngine loader; if (!loader.loadPackage()) return 1;
    QApplication::setApplicationName(loader.appName());

    // Check if already installed
    auto info = InstallerEngine::getInstalledInfo(loader.shortName());
    if (!info.installed)
        info = InstallerEngine::getInstalledInfo(loader.appName());

    int mode = 0; // 0=fresh, 1=same-version, 2=different-version

    if (info.installed) {
        if (info.version == loader.version()) {
            // Same version — ask: overwrite or repair
            int ret = QMessageBox::question(nullptr,
                QStringLiteral("已安装"),
                QStringLiteral("已安装相同版本 %1。\n\n"
                               "覆盖安装：重新安装所有文件（保留配置）\n"
                               "修复安装：仅修复缺失或损坏的文件\n\n"
                               "请选择操作：").arg(info.version),
                QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel,
                QMessageBox::Yes);
            if (ret == QMessageBox::Cancel) return 0;
            mode = (ret == QMessageBox::Yes) ? 1 : 2; // 1=overwrite, 2=repair
        } else {
            // Different version — confirm upgrade
            int ret = QMessageBox::question(nullptr,
                QStringLiteral("升级"),
                QStringLiteral("已安装 %1 (版本 %2)。\n"
                               "即将升级到 %3。是否继续？")
                    .arg(loader.appName(), info.version, loader.version()),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (ret != QMessageBox::Yes) return 0;
            mode = 1; // upgrade (overwrite)
        }

        // If overwrite/repair/upgrade → use existing path, keep config
        if (mode == 1) {
            // Overwrite: silent update preserves config
            InstallerEngine e; e.loadPackage();
            e.silentUpdate(info.installPath);
            QMessageBox::information(nullptr, QStringLiteral("完成"),
                QStringLiteral("%1 已更新到版本 %2。").arg(loader.appName(), loader.version()));
            return 0;
        }
        if (mode == 2) {
            // Repair: just re-copy files (skip registry/shortcuts)
            InstallerEngine e; e.loadPackage();
            e.install(info.installPath);
            QMessageBox::information(nullptr, QStringLiteral("完成"),
                QStringLiteral("%1 已修复。").arg(loader.appName()));
            return 0;
        }
    }

    // Fresh install — show wizard
    InstallWizard w(InstallWizard::Install, loader.package());
    w.show();
    return QApplication::exec();
}

// ═══════════════════════════════════════════════════════════════════════════════
int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // ── Parse args ────────────────────────────────────────────────────────
    QStringList args = app.arguments();

    // --install-silent <stateFile>  (admin elevation)
    if (args.size() >= 3 && args[1] == QStringLiteral("--install-silent"))
        return runSilentInstall(args[2]);

    // --uninstall <dir>
    if (args.size() >= 3 && args[1] == QStringLiteral("--uninstall"))
        return runUninstallWizard(args[2]);

    // /S or --silent  (unattended install, NSIS convention + explicit)
    if (args.contains(QStringLiteral("/S")) || args.contains(QStringLiteral("--silent")))
        return runSilentInstall();

    // --update <dir> (auto-update, keep config)
    if (args.size() >= 3 && args[1] == QStringLiteral("--update"))
        return runSilentUpdate(args[2]);

    // Auto-detect: running as Uninstall.exe in install dir
    if (tryAutoUninstall())
        return 0;

    // ── Normal ────────────────────────────────────────────────────────────
    app.setApplicationName(QStringLiteral("InstallerMaker"));
    return runInstallWizard();
}
