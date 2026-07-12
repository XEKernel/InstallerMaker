#include "configurator.hpp"
#include <QApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProcess>
#include <QStandardPaths>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("InstallerMaker"));

    const QStringList args = app.arguments();

    // ── CLI batch mode: Configurator.exe --batch <config.json> <output_dir> ──
    if (args.size() >= 4 && args[1] == QStringLiteral("--batch")) {
        QString cfgPath = args[2];
        QString outDir  = args[3];

        QFile cf(cfgPath);
        if (!cf.open(QIODevice::ReadOnly)) {
            QMessageBox::critical(nullptr, QStringLiteral("错误"),
                QStringLiteral("无法读取配置: %1").arg(cfgPath));
            return 1;
        }
        QJsonObject pkg = QJsonDocument::fromJson(cf.readAll()).object();
        cf.close();

        // Write package.json + copy files to temp package dir
        QString pkgDir = outDir + QStringLiteral("/package");
        QDir().mkpath(pkgDir);
        QFile pf(pkgDir + QStringLiteral("/package.json"));
        if (!pf.open(QIODevice::WriteOnly)) {
            return 1;
        }
        pf.write(QJsonDocument(pkg).toJson(QJsonDocument::Indented));
        pf.close();

        // Copy files if specified
        QString filesSrc = pkg.value(QStringLiteral("installer")).toObject()
                               .value(QStringLiteral("_files_source")).toString();
        if (!filesSrc.isEmpty()) {
            QString dst = pkgDir + QStringLiteral("/files");
            QDir().mkpath(dst);
            QDirIterator it(filesSrc, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                QFileInfo fi = it.fileInfo();
                QString rel = QDir(filesSrc).relativeFilePath(fi.absoluteFilePath());
                QString dest = dst + QStringLiteral("/") + rel;
                if (fi.isDir()) QDir().mkpath(dest);
                else { QDir().mkpath(QFileInfo(dest).absolutePath());
                       if (QFile::exists(dest)) QFile::remove(dest);
                       QFile::copy(fi.absoluteFilePath(), dest); }
            }
        }

        // Build using Configurator's build logic
        Configurator dlg;
        // Reach into private — use a temporary Configurator just for build
        // Since runBuild is private, we create an instance and need a workaround
        // → add a public static wrapper
        // For now: show configurator with pre-filled values would be complex.
        // Instead, just run cmake directly here.
        auto run = [&](const QString& d, const QString& p, const QStringList& a) -> bool {
            QProcess proc; proc.setWorkingDirectory(d);
            proc.start(p, a);
            proc.waitForFinished(300000);
            return proc.exitCode() == 0;
        };

        QString tmpBuild = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                           + QStringLiteral("/InstallerMaker_build");
        QDir(tmpBuild).removeRecursively(); QDir().mkpath(tmpBuild);
        QString srcDir  = QDir(QCoreApplication::applicationDirPath())
                              .absoluteFilePath(QStringLiteral("../.."));

        if (!run(tmpBuild, QStringLiteral("cmake"),
                 {QStringLiteral("-G"), QStringLiteral("Ninja"),
                  QStringLiteral("-DCMAKE_BUILD_TYPE=Release"),
                  QStringLiteral("-DPACKAGE_DIR=%1").arg(pkgDir), srcDir}))
            return 1;

        if (!run(tmpBuild, QStringLiteral("cmake"),
                 {QStringLiteral("--build"), QStringLiteral("."),
                  QStringLiteral("--config"), QStringLiteral("Release")}))
            return 1;

        QString exe = tmpBuild + QStringLiteral("/Setup.exe");
        QString dst = outDir   + QStringLiteral("/Setup.exe");
        if (QFile::exists(exe)) {
            if (QFile::exists(dst)) QFile::remove(dst);
            QFile::copy(exe, dst);
        }
        QDir(tmpBuild).removeRecursively();
        return 0;
    }

    // ── GUI mode ──────────────────────────────────────────────────────────
    Configurator dlg;
    dlg.show();
    return app.exec();
}
