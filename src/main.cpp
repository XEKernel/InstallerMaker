#include "installerengine.hpp"
#include "installwizard.hpp"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>

// ── helpers ──────────────────────────────────────────────────────────────────

static QJsonObject readJsonFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    return doc.object();
}

// ── Silent install (elevated by ShellExecuteW runas) ─────────────────────────

static int runSilentInstall(const QString& stateFilePath)
{
    QJsonObject state = readJsonFile(stateFilePath);
    if (state.isEmpty()) {
        QMessageBox::critical(nullptr, QStringLiteral("安装失败"),
            QStringLiteral("无法读取安装状态文件:\n%1").arg(stateFilePath));
        return 1;
    }

    const QString installDir   = state.value(QStringLiteral("install_dir")).toString();
    const QJsonObject package  = state.value(QStringLiteral("package")).toObject();
    if (installDir.isEmpty()) {
        QMessageBox::critical(nullptr, QStringLiteral("安装失败"),
            QStringLiteral("状态文件中缺少 install_dir"));
        return 1;
    }

    InstallerEngine engine;
    if (!engine.loadPackage()) return 1;
    engine.install(installDir);

    QMessageBox::information(nullptr, QStringLiteral("安装完成"),
        package.value(QStringLiteral("finish_message")).toString());
    return 0;
}

// ── Uninstall wizard ─────────────────────────────────────────────────────────

static int runUninstallWizard(const QString& installDir)
{
    InstallerEngine loader;
    if (!loader.loadPackage()) return 1;

    InstallWizard wizard(InstallWizard::Uninstall, loader.package(), installDir);
    wizard.exec();
    return 0;
}

// ── Auto-detect: check if uninstall.json exists next to this exe ─────────────

static bool tryAutoUninstall()
{
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString configPath = exeDir + QStringLiteral("/uninstall.json");

    QJsonObject config = readJsonFile(configPath);
    if (config.isEmpty()) return false;

    const QString installDir = config.value(QStringLiteral("install_dir")).toString();
    if (installDir.isEmpty()) return false;

    // Found uninstall.json → run as uninstaller
    return runUninstallWizard(installDir) == 0;
}

// ── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("InstallerMaker"));

    const QStringList args = app.arguments();

    // ── --install-silent <statefile> (admin elevation) ───────────────────
    if (args.size() >= 3 && args.at(1) == QStringLiteral("--install-silent"))
        return runSilentInstall(args.at(2));

    // ── --uninstall <dir> (explicit uninstall) ───────────────────────────
    if (args.size() >= 3 && args.at(1) == QStringLiteral("--uninstall"))
        return runUninstallWizard(args.at(2));

    // ── Auto-detect: Uninstall.exe with uninstall.json ───────────────────
    if (tryAutoUninstall())
        return 0;

    // ── Normal install wizard ────────────────────────────────────────────
    InstallerEngine loader;
    if (!loader.loadPackage()) return 1;

    app.setApplicationName(loader.appName());

    InstallWizard wizard(InstallWizard::Install, loader.package());
    wizard.show();

    return app.exec();
}
