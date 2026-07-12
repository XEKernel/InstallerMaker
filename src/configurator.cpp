#include "configurator.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTextEdit>
#include <QVBoxLayout>

// ═══════════════════════════════════════════════════════════════════════════════
Configurator::Configurator(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("InstallerMaker — 安装程序配置工具"));
    setMinimumSize(880, 580);
    resize(960, 640);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(16, 12, 16, 12);
    outer->setSpacing(10);

    auto* title = new QLabel(tr("<h2>配置安装程序</h2>"), this);
    outer->addWidget(title);

    // ── Two-column layout ────────────────────────────────────────────────
    auto* columns = new QHBoxLayout();
    columns->setSpacing(16);

    auto* leftCol  = new QVBoxLayout();
    auto* rightCol = new QVBoxLayout();

    buildBasicGroup(leftCol);
    buildInstallGroup(leftCol);
    buildFilesGroup(leftCol);
    buildLicenseGroup(leftCol);
    leftCol->addStretch();

    buildAdvancedGroup(rightCol);
    buildRegistryGroup(rightCol);
    buildOutputGroup(rightCol);
    rightCol->addStretch();

    columns->addLayout(leftCol);
    columns->addLayout(rightCol);
    outer->addLayout(columns, 1);

    // ── Action bar at bottom ─────────────────────────────────────────────
    buildActionBar(outer);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Section builders
// ═══════════════════════════════════════════════════════════════════════════════

void Configurator::buildBasicGroup(QVBoxLayout* main)
{
    auto* grp  = new QGroupBox(tr("基本信息"));
    auto* form = new QFormLayout(grp);
    m_nameEdit = new QLineEdit(QStringLiteral("MyApp"));
    form->addRow(tr("应用名称:"), m_nameEdit);
    m_versionEdit = new QLineEdit(QStringLiteral("1.0.0"));
    form->addRow(tr("版本号:"), m_versionEdit);
    m_publisherEdit = new QLineEdit(QStringLiteral("MyCompany"));
    form->addRow(tr("发布者:"), m_publisherEdit);
    main->addWidget(grp);
}

void Configurator::buildInstallGroup(QVBoxLayout* main)
{
    auto* grp  = new QGroupBox(tr("安装设置"));
    auto* form = new QFormLayout(grp);

    m_rootCombo = new QComboBox();
    m_rootCombo->addItem(tr("用户本地数据 (%LOCALAPPDATA%)"), QStringLiteral("$LOCALAPPDATA$"));
    m_rootCombo->addItem(tr("程序文件 (%PROGRAMFILES%)"),      QStringLiteral("$PROGRAMFILES$"));
    form->addRow(tr("默认安装根目录:"), m_rootCombo);

    m_folderEdit = new QLineEdit(QStringLiteral("MyApp"));
    form->addRow(tr("安装文件夹名:"), m_folderEdit);

    m_exeEdit = new QLineEdit(QStringLiteral("MyApp.exe"));
    form->addRow(tr("主程序文件名:"), m_exeEdit);

    auto* scRow = new QHBoxLayout();
    m_desktopBox = new QCheckBox(tr("桌面快捷方式"));
    m_desktopBox->setChecked(true);
    scRow->addWidget(m_desktopBox);
    m_startMenuBox = new QCheckBox(tr("开始菜单快捷方式"));
    m_startMenuBox->setChecked(true);
    scRow->addWidget(m_startMenuBox);
    scRow->addStretch();
    form->addRow(tr("快捷方式:"), scRow);

    main->addWidget(grp);
}

void Configurator::buildFilesGroup(QVBoxLayout* main)
{
    auto* grp = new QGroupBox(tr("打包文件"));
    auto* row = new QHBoxLayout(grp);
    m_filesEdit = new QLineEdit();
    m_filesEdit->setPlaceholderText(tr("选择包含待打包文件的目录..."));
    row->addWidget(m_filesEdit, 1);
    auto* btn = new QPushButton(tr("浏览..."));
    connect(btn, &QPushButton::clicked, this, &Configurator::onBrowseFiles);
    row->addWidget(btn);
    main->addWidget(grp);
}

void Configurator::buildLicenseGroup(QVBoxLayout* main)
{
    auto* grp = new QGroupBox(tr("许可协议（可选）"));
    auto* row = new QHBoxLayout(grp);
    m_licenseEdit = new QLineEdit();
    m_licenseEdit->setPlaceholderText(tr("选择许可协议文本文件..."));
    row->addWidget(m_licenseEdit, 1);
    auto* btn = new QPushButton(tr("浏览..."));
    connect(btn, &QPushButton::clicked, this, &Configurator::onBrowseLicense);
    row->addWidget(btn);
    main->addWidget(grp);
}

void Configurator::buildAdvancedGroup(QVBoxLayout* main)
{
    auto* grp  = new QGroupBox(tr("高级设置"));
    auto* form = new QFormLayout(grp);

    m_startMenuFolderEdit = new QLineEdit();
    m_startMenuFolderEdit->setPlaceholderText(tr("留空则使用应用名称"));
    form->addRow(tr("开始菜单文件夹名:"), m_startMenuFolderEdit);

    m_requireAdminBox = new QCheckBox(tr("强制要求管理员权限（安装到 Program Files 时建议开启）"));
    form->addRow({}, m_requireAdminBox);

    m_supportUrlEdit = new QLineEdit();
    m_supportUrlEdit->setPlaceholderText(tr("https://..."));
    form->addRow(tr("支持网址:"), m_supportUrlEdit);

    m_updateUrlEdit = new QLineEdit();
    m_updateUrlEdit->setPlaceholderText(tr("https://..."));
    form->addRow(tr("更新网址:"), m_updateUrlEdit);

    m_welcomeTextEdit = new QLineEdit();
    m_welcomeTextEdit->setPlaceholderText(tr("留空使用默认欢迎语"));
    form->addRow(tr("自定义欢迎语:"), m_welcomeTextEdit);

    m_descriptionEdit = new QLineEdit();
    m_descriptionEdit->setPlaceholderText(tr("在「添加/删除程序」中显示的描述"));
    form->addRow(tr("应用描述:"), m_descriptionEdit);

    m_autoStartBox = new QCheckBox(tr("安装后添加到开机自启"));
    form->addRow({}, m_autoStartBox);

    m_requireRestartBox = new QCheckBox(tr("安装完成后提示重启计算机"));
    form->addRow({}, m_requireRestartBox);

    m_runAfterInstallBox = new QCheckBox(tr("默认勾选「运行应用程序」"));
    m_runAfterInstallBox->setChecked(true);
    form->addRow({}, m_runAfterInstallBox);

    m_finishEdit = new QLineEdit(tr("安装完成！"));
    form->addRow(tr("安装完成提示:"), m_finishEdit);

    // App icon
    auto* iconRow = new QHBoxLayout();
    m_iconEdit = new QLineEdit();
    m_iconEdit->setPlaceholderText(tr("快捷方式图标 .ico（可选）..."));
    iconRow->addWidget(m_iconEdit, 1);
    auto* iconBtn = new QPushButton(tr("浏览..."));
    connect(iconBtn, &QPushButton::clicked, this, &Configurator::onBrowseIcon);
    iconRow->addWidget(iconBtn);
    form->addRow(tr("应用图标:"), iconRow);

    main->addWidget(grp);
}

void Configurator::buildRegistryGroup(QVBoxLayout* main)
{
    auto* grp = new QGroupBox(tr("额外注册表项（可选）"));
    auto* lay = new QVBoxLayout(grp);

    auto* hint = new QLabel(
        tr("应用自身的注册表键会自动创建。此处可添加其他自定义键值对。"));
    hint->setWordWrap(true);
    lay->addWidget(hint);

    m_regEntriesLayout = new QVBoxLayout();
    m_regEntriesLayout->setSpacing(4);
    lay->addLayout(m_regEntriesLayout);

    onAddRegEntry();

    auto* addBtn = new QPushButton(tr("+ 添加注册表项"));
    connect(addBtn, &QPushButton::clicked, this, &Configurator::onAddRegEntry);
    lay->addWidget(addBtn);

    main->addWidget(grp);
}

void Configurator::buildOutputGroup(QVBoxLayout* main)
{
    auto* grp = new QGroupBox(tr("输出"));
    auto* row = new QHBoxLayout(grp);
    m_outputEdit = new QLineEdit(
        QDir::toNativeSeparators(
            QDir(QCoreApplication::applicationDirPath())
                .absoluteFilePath(QStringLiteral("output"))));
    row->addWidget(m_outputEdit, 1);
    auto* btn = new QPushButton(tr("浏览..."));
    connect(btn, &QPushButton::clicked, this, &Configurator::onBrowseOutput);
    row->addWidget(btn);
    main->addWidget(grp);
}

void Configurator::buildActionBar(QVBoxLayout* main)
{
    auto* row = new QHBoxLayout();
    row->addStretch();

    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 0);
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumWidth(200);
    row->addWidget(m_progressBar);

    m_generateBtn = new QPushButton(tr("生成安装程序(&G)"));
    m_generateBtn->setMinimumWidth(180);
    m_generateBtn->setStyleSheet(QStringLiteral(
        "QPushButton { font-weight: bold; padding: 8px 16px; }"));
    connect(m_generateBtn, &QPushButton::clicked, this, &Configurator::onGenerate);
    row->addWidget(m_generateBtn);

    main->addLayout(row);

    m_logEdit = new QTextEdit();
    m_logEdit->setReadOnly(true);
    m_logEdit->setMaximumHeight(140);
    m_logEdit->setPlaceholderText(tr("构建日志将显示在此处..."));
    main->addWidget(m_logEdit);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Slots
// ═══════════════════════════════════════════════════════════════════════════════

void Configurator::onBrowseFiles()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择待打包文件目录"));
    if (!dir.isEmpty()) m_filesEdit->setText(QDir::toNativeSeparators(dir));
}

void Configurator::onBrowseLicense()
{
    QString f = QFileDialog::getOpenFileName(this, tr("选择许可协议"),
        {}, tr("文本文件 (*.txt);;所有文件 (*)"));
    if (!f.isEmpty()) m_licenseEdit->setText(QDir::toNativeSeparators(f));
}

void Configurator::onBrowseIcon()
{
    QString f = QFileDialog::getOpenFileName(this, tr("选择图标文件"),
        {}, tr("图标文件 (*.ico);;所有文件 (*)"));
    if (!f.isEmpty()) m_iconEdit->setText(QDir::toNativeSeparators(f));
}

void Configurator::onBrowseOutput()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("选择输出目录"));
    if (!dir.isEmpty()) m_outputEdit->setText(QDir::toNativeSeparators(dir));
}

void Configurator::onAddRegEntry()
{
    auto* row = new QHBoxLayout();
    row->setSpacing(6);

    auto* keyEdit = new QLineEdit();
    keyEdit->setPlaceholderText(tr("键名 (如 HKCU\\Software\\MyApp\\Setting)"));
    row->addWidget(keyEdit, 2);

    auto* valEdit = new QLineEdit();
    valEdit->setPlaceholderText(tr("值"));
    row->addWidget(valEdit, 3);

    auto* delBtn = new QPushButton(tr("✕"));
    delBtn->setFixedWidth(30);
    delBtn->setToolTip(tr("删除此项"));

    int idx = static_cast<int>(m_regRows.size());
    RegRow r{keyEdit, valEdit};
    m_regRows.append(r);

    connect(delBtn, &QPushButton::clicked, this, [this, row, idx,
             keyEdit, valEdit, delBtn]() {
        m_regEntriesLayout->removeItem(row);
        m_regRows[idx] = RegRow{nullptr, nullptr};  // mark removed
        delete keyEdit;
        delete valEdit;
        delete delBtn;
        delete row;
    });
    row->addWidget(delBtn);

    m_regEntriesLayout->addLayout(row);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Generate package.json + copy files
// ═══════════════════════════════════════════════════════════════════════════════

bool Configurator::generatePackageJson(const QString& packageDir)
{
    const QString name      = m_nameEdit->text().trimmed();
    const QString version   = m_versionEdit->text().trimmed();
    const QString publisher = m_publisherEdit->text().trimmed();
    const QString root      = m_rootCombo->currentData().toString();
    const QString folder    = m_folderEdit->text().trimmed();
    const QString exe       = m_exeEdit->text().trimmed();
    const QString license   = m_licenseEdit->text().trimmed();
    const QString filesDir  = m_filesEdit->text().trimmed();
    const QString iconPath  = m_iconEdit->text().trimmed();

    if (name.isEmpty() || folder.isEmpty() || exe.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("应用名称、安装文件夹和主程序文件名不能为空。"));
        return false;
    }

    QJsonObject inst;
    inst[QStringLiteral("name")]                  = name;
    inst[QStringLiteral("version")]               = version;
    inst[QStringLiteral("publisher")]             = publisher;
    inst[QStringLiteral("default_install_folder")] = folder;
    inst[QStringLiteral("default_install_root")]  = root;
    inst[QStringLiteral("main_executable")]        = exe;

    const QString smFolder = m_startMenuFolderEdit->text().trimmed();
    if (!smFolder.isEmpty())
        inst[QStringLiteral("start_menu_folder")] = smFolder;

    if (m_requireAdminBox->isChecked())
        inst[QStringLiteral("require_admin")] = true;

    const QString support = m_supportUrlEdit->text().trimmed();
    if (!support.isEmpty())
        inst[QStringLiteral("support_url")] = support;

    const QString update = m_updateUrlEdit->text().trimmed();
    if (!update.isEmpty())
        inst[QStringLiteral("update_url")] = update;

    const QString welcome = m_welcomeTextEdit->text().trimmed();
    if (!welcome.isEmpty())
        inst[QStringLiteral("welcome_text")] = welcome;

    if (!iconPath.isEmpty()) {
        QFileInfo ifi(iconPath);
        inst[QStringLiteral("app_icon")] = ifi.fileName();
    }

    const QString desc = m_descriptionEdit->text().trimmed();
    if (!desc.isEmpty())
        inst[QStringLiteral("description")] = desc;

    if (m_autoStartBox->isChecked())
        inst[QStringLiteral("auto_start")] = true;

    if (m_requireRestartBox->isChecked())
        inst[QStringLiteral("require_restart")] = true;

    inst[QStringLiteral("run_after_install")] = m_runAfterInstallBox->isChecked();

    QJsonObject pkg;
    pkg[QStringLiteral("installer")] = inst;

    QJsonArray shortcuts;
    if (m_desktopBox->isChecked())
        shortcuts.append(QJsonObject{{QStringLiteral("target"), exe},
                                     {QStringLiteral("location"), QStringLiteral("Desktop")}});
    if (m_startMenuBox->isChecked())
        shortcuts.append(QJsonObject{{QStringLiteral("target"), exe},
                                     {QStringLiteral("location"), QStringLiteral("StartMenu")}});
    pkg[QStringLiteral("shortcuts")] = shortcuts;

    QJsonObject registry;
    QJsonObject appKey;
    appKey[QStringLiteral("Version")] = QStringLiteral("{VERSION}");
    appKey[QStringLiteral("Path")]    = QStringLiteral("{INSTALL_DIR}");
    registry[QStringLiteral("HKEY_CURRENT_USER\\Software\\%1").arg(name)] = appKey;

    for (const auto& row : m_regRows) {
        if (!row.key) continue;  // removed entry
        QString k = row.key->text().trimmed();
        QString v = row.value->text().trimmed();
        if (!k.isEmpty() && !v.isEmpty())
            registry[k] = QJsonObject{{QStringLiteral(""), v}};
    }

    pkg[QStringLiteral("registry")] = registry;

    if (!license.isEmpty()) {
        QFileInfo lfi(license);
        pkg[QStringLiteral("license_file")] = lfi.fileName();
    }

    pkg[QStringLiteral("finish_message")] = m_finishEdit->text().trimmed();

    QDir().mkpath(packageDir);
    QFile pf(packageDir + QStringLiteral("/package.json"));
    if (!pf.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("错误"),
            tr("无法写入 package.json:\n%1").arg(pf.fileName()));
        return false;
    }
    pf.write(QJsonDocument(pkg).toJson(QJsonDocument::Indented));
    pf.close();

    const QString targetFiles = packageDir + QStringLiteral("/files");
    QDir().mkpath(targetFiles);

    if (!filesDir.isEmpty()) {
        QDirIterator it(filesDir, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            QFileInfo fi = it.fileInfo();
            QString rel  = QDir(filesDir).relativeFilePath(fi.absoluteFilePath());
            QString dest = targetFiles + QStringLiteral("/") + rel;
            if (fi.isDir())
                QDir().mkpath(dest);
            else {
                QDir().mkpath(QFileInfo(dest).absolutePath());
                if (QFile::exists(dest)) QFile::remove(dest);
                QFile::copy(fi.absoluteFilePath(), dest);
            }
        }
    }

    if (!license.isEmpty()) {
        QFileInfo lfi(license);
        QString dest = targetFiles + QStringLiteral("/") + lfi.fileName();
        if (QFile::exists(dest)) QFile::remove(dest);
        QFile::copy(license, dest);
    }

    if (!iconPath.isEmpty()) {
        QFileInfo ifi(iconPath);
        QString dest = targetFiles + QStringLiteral("/") + ifi.fileName();
        if (QFile::exists(dest)) QFile::remove(dest);
        QFile::copy(iconPath, dest);
    }

    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Build (async, temp dir, no CJK)
// ═══════════════════════════════════════════════════════════════════════════════

bool Configurator::runBuild(const QString& packageDir, const QString& outputDir)
{
    m_progressBar->setVisible(true);
    m_generateBtn->setEnabled(false);
    m_logEdit->clear();
    QApplication::processEvents();

    const QString sourceDir = QDir(QCoreApplication::applicationDirPath())
                                  .absoluteFilePath(QStringLiteral(".."));
    const QString tmpBuild  = QStandardPaths::writableLocation(
                                  QStandardPaths::TempLocation)
                              + QStringLiteral("/InstallerMaker_build");
    QDir(tmpBuild).removeRecursively();
    QDir().mkpath(tmpBuild);

    auto runProc = [this](const QString& dir, const QString& program,
                          const QStringList& args) -> bool
    {
        QProcess proc;
        proc.setWorkingDirectory(dir);
        QObject::connect(&proc, &QProcess::readyReadStandardOutput, this,
            [this, &proc]() {
                QString s = QString::fromLocal8Bit(proc.readAllStandardOutput());
                if (!s.trimmed().isEmpty()) m_logEdit->append(s.trimmed());
            });
        QObject::connect(&proc, &QProcess::readyReadStandardError, this,
            [this, &proc]() {
                QString s = QString::fromLocal8Bit(proc.readAllStandardError());
                if (!s.trimmed().isEmpty()) m_logEdit->append(s.trimmed());
            });
        QEventLoop loop;
        QObject::connect(&proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                         &loop, &QEventLoop::quit);
        proc.start(program, args);
        if (!proc.waitForStarted(10000)) {
            m_logEdit->append(tr("[错误] 无法启动: %1").arg(program));
            return false;
        }
        loop.exec();
        return proc.exitCode() == 0;
    };

    m_logEdit->append(tr("═══ 配置 CMake... ═══"));
    QApplication::processEvents();
    if (!runProc(tmpBuild, QStringLiteral("cmake"),
                 {QStringLiteral("-G"), QStringLiteral("Ninja"),
                  QStringLiteral("-DCMAKE_BUILD_TYPE=Release"),
                  QStringLiteral("-DPACKAGE_DIR=%1").arg(packageDir),
                  sourceDir}))
        goto fail;

    m_logEdit->append(tr("═══ 开始编译... ═══"));
    QApplication::processEvents();
    if (!runProc(tmpBuild, QStringLiteral("cmake"),
                 {QStringLiteral("--build"), QStringLiteral("."),
                  QStringLiteral("--config"), QStringLiteral("Release")}))
        goto fail;

    {
        const QString src = tmpBuild + QStringLiteral("/Setup.exe");
        const QString dst = outputDir + QStringLiteral("/Setup.exe");
        if (QFile::exists(src)) {
            if (QFile::exists(dst)) QFile::remove(dst);
            QFile::copy(src, dst);
            m_logEdit->append(tr("═══ 生成完成! ═══"));
            m_logEdit->append(tr("Setup.exe → %1").arg(QDir::toNativeSeparators(dst)));
        } else {
            m_logEdit->append(tr("[错误] 未找到 Setup.exe"));
            goto fail;
        }
    }

    QDir(tmpBuild).removeRecursively();
    m_progressBar->setVisible(false);
    m_generateBtn->setEnabled(true);
    QMessageBox::information(this, tr("成功"),
        tr("安装程序已生成:\n%1").arg(
            QDir::toNativeSeparators(outputDir + QStringLiteral("/Setup.exe"))));
    return true;

fail:
    m_progressBar->setVisible(false);
    m_generateBtn->setEnabled(true);
    QMessageBox::critical(this, tr("构建失败"), tr("构建失败，请检查日志。"));
    return false;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Generate action
// ═══════════════════════════════════════════════════════════════════════════════

void Configurator::onGenerate()
{
    const QString outputDir  = m_outputEdit->text().trimmed();
    const QString packageDir = outputDir + QStringLiteral("/package");

    if (outputDir.isEmpty()) {
        QMessageBox::warning(this, tr("错误"), tr("请选择输出目录。"));
        return;
    }
    if (!generatePackageJson(packageDir))
        return;
    runBuild(packageDir, outputDir);
}
