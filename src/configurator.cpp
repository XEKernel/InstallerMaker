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
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

// ═══════════════════════════════════════════════════════════════════════════════
Configurator::Configurator(QWidget* parent) : QDialog(parent)
{
    setWindowTitle(tr("InstallerMaker — 安装程序配置工具"));
    setMinimumSize(760, 620);
    resize(800, 660);
    setupUi();
}

void Configurator::setupUi()
{
    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(10, 10, 10, 10);
    main->addWidget(new QLabel(tr("<h2>配置安装程序</h2>"), this));

    auto* tabs = new QTabWidget(this);
    tabs->addTab(buildTabBasic(),       tr("基本信息"));
    tabs->addTab(buildTabInstall(),     tr("安装设置"));
    tabs->addTab(buildTabFiles(),       tr("文件策略"));
    tabs->addTab(buildTabShortcuts(),   tr("快捷方式"));
    tabs->addTab(buildTabAppearance(),  tr("外观"));
    tabs->addTab(buildTabRegistry(),    tr("注册表"));
    tabs->addTab(buildTabUninstall(),   tr("卸载项"));
    tabs->addTab(buildTabEnvironment(), tr("环境变量"));
    tabs->addTab(buildTabAssociations(),tr("文件关联"));
    tabs->addTab(buildTabFinish(),      tr("完成动作"));
    tabs->addTab(buildTabAdvanced(),    tr("高级"));
    tabs->addTab(buildTabOutput(),      tr("输出"));
    main->addWidget(tabs, 1);

    // Action bar
    auto* bar = new QHBoxLayout();
    bar->addStretch();
    m_progressBar = new QProgressBar(); m_progressBar->setRange(0,0);
    m_progressBar->setVisible(false); m_progressBar->setMaximumWidth(180);
    bar->addWidget(m_progressBar);
    m_generateBtn = new QPushButton(tr("生成安装程序(&G)"));
    m_generateBtn->setMinimumWidth(180);
    m_generateBtn->setStyleSheet(QStringLiteral("QPushButton{font-weight:bold;padding:8px 16px}"));
    connect(m_generateBtn, &QPushButton::clicked, this, &Configurator::onGenerate);
    bar->addWidget(m_generateBtn);
    main->addLayout(bar);

    m_log = new QTextEdit(); m_log->setReadOnly(true); m_log->setMaximumHeight(120);
    m_log->setPlaceholderText(tr("构建日志将显示在此处..."));
    main->addWidget(m_log);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Tabs
// ═══════════════════════════════════════════════════════════════════════════════

static QFormLayout* form(QWidget* p) { auto* f=new QFormLayout(p); f->setContentsMargins(8,8,8,8); return f; }
static void addBrowse(QFormLayout* f, QLineEdit* e, const QString& title, bool dir=false) {
    auto* b=new QPushButton(QObject::tr("浏览..."));
    QObject::connect(b, &QPushButton::clicked, e, [=](){
        QString s = dir ? QFileDialog::getExistingDirectory(nullptr, title)
                        : QFileDialog::getOpenFileName(nullptr, title, {}, {});
        if (!s.isEmpty()) e->setText(QDir::toNativeSeparators(s));
    });
    f->addRow({}, b);
}

QWidget* Configurator::buildTabBasic()
{
    auto* w = new QWidget; auto* f = form(w);
    m_name      = new QLineEdit(QStringLiteral("MyApp"));    f->addRow(tr("产品名:"), m_name);
    m_shortName = new QLineEdit(); m_shortName->setPlaceholderText(tr("自动生成")); f->addRow(tr("短名称:"), m_shortName);
    m_version   = new QLineEdit(QStringLiteral("1.0.0.0"));  f->addRow(tr("版本:"), m_version);
    m_publisher = new QLineEdit(QStringLiteral("MyCompany")); f->addRow(tr("发布者:"), m_publisher);
    m_copyright = new QLineEdit(QStringLiteral("Copyright (C) 2026")); f->addRow(tr("版权:"), m_copyright);
    m_website   = new QLineEdit(); m_website->setPlaceholderText(tr("https://...")); f->addRow(tr("网址:"), m_website);
    m_email     = new QLineEdit(); m_email->setPlaceholderText(tr("support@..."));   f->addRow(tr("支持邮箱:"), m_email);

    // Auto-sync name → short_name, folder, exe
    connect(m_name, &QLineEdit::textChanged, this, [this](const QString& t) {
        QString n = t.trimmed();
        QString sn = n.toLower().replace(QRegularExpression(QStringLiteral("[^a-z0-9_]")), QStringLiteral("_"));
        if (m_shortName->text().isEmpty() || m_shortName->text() == m_name->text().toLower().replace(QRegularExpression(QStringLiteral("[^a-z0-9_]")), QStringLiteral("_")))
            m_shortName->setText(sn);
        if (m_folder->text().isEmpty() || m_folder->text() == sn)
            m_folder->setText(n.isEmpty() ? QString() : n);
        if (m_exe->text().isEmpty() || m_exe->text() == sn + QStringLiteral(".exe"))
            m_exe->setText(n.isEmpty() ? QString() : n + QStringLiteral(".exe"));
    });
    return w;
}

QWidget* Configurator::buildTabInstall()
{
    auto* w = new QWidget; auto* f = form(w);
    m_rootCombo = new QComboBox;
    m_rootCombo->addItem(tr("用户本地数据 (%LOCALAPPDATA%)"), QStringLiteral("$LOCALAPPDATA$"));
    m_rootCombo->addItem(tr("程序文件 (%PROGRAMFILES%)"),      QStringLiteral("$PROGRAMFILES$"));
    m_rootCombo->addItem(tr("程序文件 x86 (%PROGRAMFILES32%)"), QStringLiteral("$PROGRAMFILES32$"));
    m_rootCombo->addItem(tr("Roaming (%APPDATA%)"),            QStringLiteral("$APPDATA$"));
    f->addRow(tr("安装根目录:"), m_rootCombo);
    m_folder = new QLineEdit(QStringLiteral("MyApp"));        f->addRow(tr("安装文件夹:"), m_folder);
    m_exe    = new QLineEdit(QStringLiteral("MyApp.exe"));     f->addRow(tr("主程序:"), m_exe);
    m_license = new QLineEdit; m_license->setPlaceholderText(tr("许可协议文件（可选）"));
    f->addRow(tr("许可协议:"), m_license);
    addBrowse(f, m_license, tr("选择许可协议"));
    m_reqAdmin  = new QCheckBox(tr("强制管理员权限")); f->addRow({}, m_reqAdmin);
    m_allowPath = new QCheckBox(tr("允许用户修改安装路径")); m_allowPath->setChecked(true); f->addRow({}, m_allowPath);
    return w;
}

QWidget* Configurator::buildTabFiles()
{
    auto* w = new QWidget; auto* f = form(w);
    m_filesDir = new QLineEdit; m_filesDir->setPlaceholderText(tr("待打包文件目录"));
    f->addRow(tr("文件目录:"), m_filesDir);
    addBrowse(f, m_filesDir, tr("选择文件目录"), true);
    m_fileMode = new QComboBox; m_fileMode->addItems({QStringLiteral("mirror"), QStringLiteral("mapping")});
    f->addRow(tr("模式:"), m_fileMode);
    m_overwrite = new QComboBox; m_overwrite->addItems({tr("询问"), tr("总是覆盖"), tr("跳过")});
    m_overwrite->setCurrentIndex(0);
    f->addRow(tr("覆盖策略:"), m_overwrite);
    m_exclude = new QLineEdit; m_exclude->setPlaceholderText(tr("逗号分隔，如 *.pdb, temp/**"));
    f->addRow(tr("排除文件:"), m_exclude);
    return w;
}

QWidget* Configurator::buildTabShortcuts()
{
    auto* w = new QWidget; auto* lay = new QVBoxLayout(w);
    auto* add = new QPushButton(tr("+ 添加快捷方式"));
    lay->addWidget(add);
    m_scLayout = new QVBoxLayout; lay->addLayout(m_scLayout);
    lay->addStretch();
    connect(add, &QPushButton::clicked, this, &Configurator::onAddShortcut);
    // Default: Desktop + StartMenu
    onAddShortcut(); onAddShortcut();
    return w;
}

void Configurator::onAddShortcut()
{
    auto* row = new QHBoxLayout; row->setSpacing(4);
    auto* t = new QLineEdit(QStringLiteral("MyApp.exe")); t->setPlaceholderText(tr("目标"));
    auto* l = new QComboBox; l->addItems({QStringLiteral("Desktop"), QStringLiteral("StartMenu"), QStringLiteral("Startup")});
    auto* nm = new QLineEdit; nm->setPlaceholderText(tr("名称"));
    auto* d = new QLineEdit; d->setPlaceholderText(tr("描述"));
    auto* a = new QLineEdit; a->setPlaceholderText(tr("参数"));
    auto* wd = new QLineEdit; wd->setPlaceholderText(tr("工作目录"));
    auto* ic = new QLineEdit; ic->setPlaceholderText(tr("图标"));
    auto* sf = new QLineEdit; sf->setPlaceholderText(tr("开始菜单文件夹"));
    auto* ad = new QCheckBox(tr("以管理员运行"));
    auto* del = new QPushButton(tr("✕")); del->setFixedWidth(28);
    row->addWidget(t); row->addWidget(l); row->addWidget(nm); row->addWidget(d);
    row->addWidget(a); row->addWidget(wd); row->addWidget(ic); row->addWidget(sf);
    row->addWidget(ad); row->addWidget(del);
    m_scLayout->addLayout(row);
    int idx = static_cast<int>(m_scRows.size());
    m_scRows.append({t,l,nm,d,a,wd,ic,sf,ad});
    connect(del, &QPushButton::clicked, this, [=](){
        m_scRows[idx] = {}; m_scLayout->removeItem(row);
        while (row->count()) { auto* it = row->takeAt(0); delete it->widget(); delete it; }
        delete row;
    });
}

QWidget* Configurator::buildTabAppearance()
{
    auto* w = new QWidget; auto* f = form(w);
    m_icon       = new QLineEdit; f->addRow(tr("窗口图标:"), m_icon); addBrowse(f, m_icon, tr("选择图标"));
    m_setupIcon  = new QLineEdit; f->addRow(tr("Setup 图标:"), m_setupIcon); addBrowse(f, m_setupIcon, tr("选择图标"));
    m_uninstIcon = new QLineEdit; f->addRow(tr("卸载图标:"), m_uninstIcon); addBrowse(f, m_uninstIcon, tr("选择图标"));
    m_banner     = new QLineEdit; f->addRow(tr("横幅图片:"), m_banner); addBrowse(f, m_banner, tr("选择图片"));
    m_bgColor    = new QLineEdit(QStringLiteral("#FFFFFF")); f->addRow(tr("背景色:"), m_bgColor);
    m_uiStyle    = new QComboBox;
    m_uiStyle->addItem(tr("向导样式（分步）"), QStringLiteral("wizard"));
    m_uiStyle->addItem(tr("一键安装（单页）"), QStringLiteral("oneclick"));
    f->addRow(tr("界面样式:"), m_uiStyle);
    m_welcomeTitle = new QLineEdit; m_welcomeTitle->setPlaceholderText(tr("默认: 欢迎使用 {name} 安装向导"));
    f->addRow(tr("欢迎标题:"), m_welcomeTitle);
    m_welcomeText  = new QLineEdit; m_welcomeText->setPlaceholderText(tr("默认介绍文本"));
    f->addRow(tr("欢迎正文:"), m_welcomeText);
    m_finishTitle  = new QLineEdit; m_finishTitle->setPlaceholderText(tr("默认: 安装完成"));
    f->addRow(tr("完成标题:"), m_finishTitle);
    m_finishMsg    = new QLineEdit(tr("安装完成！")); f->addRow(tr("完成消息:"), m_finishMsg);
    return w;
}

QWidget* Configurator::buildTabRegistry()
{
    auto* w = new QWidget; auto* lay = new QVBoxLayout(w);
    auto* add = new QPushButton(tr("+ 添加注册表项")); lay->addWidget(add);
    m_regLayout = new QVBoxLayout; lay->addLayout(m_regLayout); lay->addStretch();
    connect(add, &QPushButton::clicked, this, &Configurator::onAddRegEntry);
    onAddRegEntry();
    return w;
}

void Configurator::onAddRegEntry()
{
    auto* row = new QHBoxLayout; row->setSpacing(4);
    auto* k = new QLineEdit; k->setPlaceholderText(tr("键名 (如 HKCU\\Software\\MyApp)"));
    auto* v = new QLineEdit; v->setPlaceholderText(tr("值"));
    auto* del = new QPushButton(tr("✕")); del->setFixedWidth(28);
    row->addWidget(k,2); row->addWidget(v,3); row->addWidget(del);
    m_regLayout->addLayout(row);
    int idx = static_cast<int>(m_regRows.size());
    m_regRows.append(Configurator::RegRow{k,v});
    connect(del, &QPushButton::clicked, this, [=](){
        m_regRows[idx]={nullptr,nullptr}; m_regLayout->removeItem(row);
        while (row->count()) { auto* it=row->takeAt(0); delete it->widget(); delete it; } delete row;
    });
}

QWidget* Configurator::buildTabUninstall()
{
    auto* w = new QWidget; auto* f = form(w);
    m_uninstString    = new QLineEdit; m_uninstString->setPlaceholderText(tr("如 \"{INSTALL_DIR}\\uninstall.exe\""));
    f->addRow(tr("卸载命令:"), m_uninstString);
    m_quietUninst     = new QLineEdit; f->addRow(tr("静默卸载:"), m_quietUninst);
    m_uninstDispIcon  = new QLineEdit; f->addRow(tr("显示图标:"), m_uninstDispIcon);
    m_estSize = new QSpinBox; m_estSize->setRange(0, 9999999); m_estSize->setSuffix(QStringLiteral(" KB"));
    m_estSize->setSpecialValueText(tr("自动计算")); f->addRow(tr("估计大小:"), m_estSize);
    m_noModify = new QCheckBox(tr("禁用「更改」按钮")); m_noModify->setChecked(true); f->addRow({}, m_noModify);
    m_noRepair = new QCheckBox(tr("禁用「修复」按钮")); m_noRepair->setChecked(true); f->addRow({}, m_noRepair);
    m_helpLink = new QLineEdit; f->addRow(tr("帮助链接:"), m_helpLink);
    return w;
}

QWidget* Configurator::buildTabEnvironment()
{
    auto* w = new QWidget; auto* f = form(w);
    m_userPath = new QLineEdit; m_userPath->setPlaceholderText(tr("{INSTALL_DIR}\\bin"));
    f->addRow(tr("用户 PATH 追加:"), m_userPath);
    m_userVars = new QLineEdit; m_userVars->setPlaceholderText(tr("键=值, 键2=值2"));
    f->addRow(tr("用户变量:"), m_userVars);
    m_sysPath  = new QLineEdit; m_sysPath->setPlaceholderText(tr("需要管理员权限"));
    f->addRow(tr("系统 PATH 追加:"), m_sysPath);
    m_sysVars  = new QLineEdit; f->addRow(tr("系统变量:"), m_sysVars);
    return w;
}

QWidget* Configurator::buildTabAssociations()
{
    auto* w = new QWidget; auto* lay = new QVBoxLayout(w);
    auto* add = new QPushButton(tr("+ 添加文件关联")); lay->addWidget(add);
    m_assocLayout = new QVBoxLayout; lay->addLayout(m_assocLayout); lay->addStretch();
    connect(add, &QPushButton::clicked, this, &Configurator::onAddFileAssoc);
    return w;
}

void Configurator::onAddFileAssoc()
{
    auto* row = new QHBoxLayout; row->setSpacing(4);
    auto* e  = new QLineEdit; e->setPlaceholderText(tr(".ext"));
    auto* d  = new QLineEdit; d->setPlaceholderText(tr("描述"));
    auto* c  = new QLineEdit; c->setPlaceholderText(tr("打开命令"));
    auto* ic = new QLineEdit; ic->setPlaceholderText(tr("图标"));
    auto* p  = new QLineEdit; p->setPlaceholderText(tr("ProgID（自动）"));
    auto* del = new QPushButton(tr("✕")); del->setFixedWidth(28);
    row->addWidget(e); row->addWidget(d); row->addWidget(c); row->addWidget(ic); row->addWidget(p); row->addWidget(del);
    m_assocLayout->addLayout(row);
    int idx = static_cast<int>(m_assocRows.size());
    m_assocRows.append({e,d,c,ic,p});
    connect(del, &QPushButton::clicked, this, [=](){ m_assocRows[idx]={};
        m_assocLayout->removeItem(row); while(row->count()){auto* it=row->takeAt(0); delete it->widget(); delete it;} delete row; });
}

QWidget* Configurator::buildTabFinish()
{
    auto* w = new QWidget; auto* f = form(w);
    m_runProg = new QCheckBox(tr("默认勾选「运行程序」")); m_runProg->setChecked(true); f->addRow({}, m_runProg);
    m_runArgs = new QLineEdit; m_runArgs->setPlaceholderText(tr("--first-run")); f->addRow(tr("启动参数:"), m_runArgs);
    m_openReadme = new QLineEdit; f->addRow(tr("打开自述文件:"), m_openReadme);
    m_restart = new QCheckBox(tr("提示需要重启")); f->addRow({}, m_restart);
    return w;
}

QWidget* Configurator::buildTabAdvanced()
{
    auto* w = new QWidget; auto* f = form(w);
    m_createUninst = new QCheckBox(tr("生成卸载程序")); m_createUninst->setChecked(true); f->addRow({}, m_createUninst);
    m_enableLog    = new QCheckBox(tr("启用安装日志")); f->addRow({}, m_enableLog);
    m_silentInst   = new QCheckBox(tr("支持静默安装 /S")); f->addRow({}, m_silentInst);
    m_minOs        = new QLineEdit; m_minOs->setPlaceholderText(tr("如 10.0.17763")); f->addRow(tr("最低系统版本:"), m_minOs);
    m_64bitOnly    = new QCheckBox(tr("仅64位系统")); f->addRow({}, m_64bitOnly);
    m_preservePats = new QLineEdit; m_preservePats->setPlaceholderText(tr("逗号分隔，如 *.ini, config/*, data/*")); f->addRow(tr("更新时保留文件:"), m_preservePats);
    return w;
}

QWidget* Configurator::buildTabOutput()
{
    auto* w = new QWidget;
    auto* lay = new QVBoxLayout(w);

    auto* f = new QFormLayout; f->setContentsMargins(8,8,8,8);
    m_output = new QLineEdit(QDir::toNativeSeparators(QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("output"))));
    f->addRow(tr("输出目录:"), m_output);
    addBrowse(f, m_output, tr("选择输出目录"), true);
    lay->addLayout(f);

    // ── Templates ────────────────────────────────────────────────────────
    auto* tplGrp = new QGroupBox(tr("模板"), w);
    auto* tplLay = new QVBoxLayout(tplGrp);

    m_templateList = new QListWidget(w);
    m_templateList->setMaximumHeight(100);
    tplLay->addWidget(m_templateList);

    auto* tplBtns = new QHBoxLayout;
    auto* saveBtn  = new QPushButton(tr("保存当前为模板"));
    auto* loadBtn  = new QPushButton(tr("加载选中模板"));
    auto* delBtn   = new QPushButton(tr("删除选中模板"));
    tplBtns->addWidget(saveBtn);
    tplBtns->addWidget(loadBtn);
    tplBtns->addWidget(delBtn);
    tplBtns->addStretch();
    tplLay->addLayout(tplBtns);

    connect(saveBtn, &QPushButton::clicked, this, &Configurator::onSaveTemplate);
    connect(loadBtn, &QPushButton::clicked, this, &Configurator::onLoadTemplate);
    connect(delBtn,  &QPushButton::clicked, this, [this]() {
        auto* item = m_templateList->currentItem();
        if (!item) return;
        QFile::remove(templateDir() + "/" + item->text() + ".json");
        refreshTemplateList();
    });

    lay->addWidget(tplGrp);
    lay->addStretch();

    refreshTemplateList();
    return w;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Slots
// ═══════════════════════════════════════════════════════════════════════════════

void Configurator::onBrowseFiles()
{ QString d = QFileDialog::getExistingDirectory(this, tr("选择文件目录")); if (!d.isEmpty()) m_filesDir->setText(QDir::toNativeSeparators(d)); }
void Configurator::onBrowseLicense() { onBrowseFiles(); }
void Configurator::onBrowseIcon() { QString f=QFileDialog::getOpenFileName(this,tr("选择图标"),{},tr("图标(*.ico);;所有(*)")); if(!f.isEmpty()) m_icon->setText(QDir::toNativeSeparators(f)); }
void Configurator::onBrowseOutput() { QString d=QFileDialog::getExistingDirectory(this,tr("选择输出目录")); if(!d.isEmpty()) m_output->setText(QDir::toNativeSeparators(d)); }

// ═══════════════════════════════════════════════════════════════════════════════
//  Templates
// ═══════════════════════════════════════════════════════════════════════════════

QString Configurator::templateDir() const
{
    QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                + QStringLiteral("/templates");
    QDir().mkpath(d);
    return d;
}

void Configurator::refreshTemplateList()
{
    m_templateList->clear();
    QDir dir(templateDir());
    for (const auto& fi : dir.entryInfoList({QStringLiteral("*.json")}, QDir::Files, QDir::Name)) {
        m_templateList->addItem(fi.completeBaseName());
    }
}

void Configurator::onSaveTemplate()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, tr("保存模板"),
        tr("模板名称:"), QLineEdit::Normal,
        m_name->text().trimmed(), &ok);
    if (!ok || name.isEmpty()) return;

    QJsonObject data = collectFormData();
    QFile f(templateDir() + "/" + name + ".json");
    if (!f.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("错误"), tr("无法保存模板。"));
        return;
    }
    f.write(QJsonDocument(data).toJson(QJsonDocument::Indented));
    f.close();
    refreshTemplateList();
}

void Configurator::onLoadTemplate()
{
    auto* item = m_templateList->currentItem();
    if (!item) {
        QMessageBox::information(this, tr("提示"), tr("请先在模板列表中选中一个模板。"));
        return;
    }

    QFile f(templateDir() + "/" + item->text() + ".json");
    if (!f.open(QIODevice::ReadOnly)) return;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    f.close();
    if (doc.isNull()) return;

    applyFormData(doc.object());
    refreshTemplateList();
}

QJsonObject Configurator::collectFormData() const
{
    QJsonObject o;
    auto s = [](const QString& x){ return x.trimmed(); };

    QJsonObject inst;
    inst["name"]=s(m_name->text()); inst["version"]=s(m_version->text()); inst["publisher"]=s(m_publisher->text());
    inst["short_name"]=s(m_shortName->text()); inst["copyright"]=s(m_copyright->text());
    inst["website"]=s(m_website->text()); inst["support_email"]=s(m_email->text());
    inst["default_install_root"]=m_rootCombo->currentData().toString();
    inst["default_install_folder"]=s(m_folder->text()); inst["main_executable"]=s(m_exe->text());
    inst["license_file"]=s(m_license->text());
    inst["requires_admin"]=m_reqAdmin->isChecked(); inst["allow_change_path"]=m_allowPath->isChecked();
    inst["icon"]=s(m_icon->text()); inst["setup_icon"]=s(m_setupIcon->text());
    inst["uninstall_icon"]=s(m_uninstIcon->text()); inst["banner_image"]=s(m_banner->text());
    inst["background_color"]=s(m_bgColor->text());
    inst["ui_style"]=m_uiStyle->currentData().toString();
    inst["welcome_title"]=s(m_welcomeTitle->text()); inst["welcome_text"]=s(m_welcomeText->text());
    inst["finish_title"]=s(m_finishTitle->text());
    o["installer"]=inst;
    o["finish_message"]=s(m_finishMsg->text());

    QJsonObject fs;
    fs["mode"]=m_fileMode->currentText();
    fs["overwrite"]=(m_overwrite->currentIndex()==1)?"always":(m_overwrite->currentIndex()==2)?"never":"ask";
    if(!s(m_exclude->text()).isEmpty()){ QJsonArray a; for(auto& p:s(m_exclude->text()).split(',')){ QString t=p.trimmed(); if(!t.isEmpty()) a.append(t); } fs["exclude"]=a; }
    o["files_strategy"]=fs;
    o["_files_source"]=s(m_filesDir->text());
    o["_license_source"]=s(m_license->text());

    // Store shortcuts/registry/assoc as-is from current rows
    QJsonArray scs;
    for(const auto& r:m_scRows){ if(!r.target||s(r.target->text()).isEmpty()) continue;
        QJsonObject sc; sc["target"]=s(r.target->text()); sc["location"]=r.location->currentText();
        if(!s(r.name->text()).isEmpty()) sc["name"]=s(r.name->text());
        if(!s(r.desc->text()).isEmpty()) sc["description"]=s(r.desc->text());
        if(!s(r.args->text()).isEmpty()) sc["arguments"]=s(r.args->text());
        if(!s(r.wd->text()).isEmpty()) sc["working_dir"]=s(r.wd->text());
        if(!s(r.icon->text()).isEmpty()) sc["icon"]=s(r.icon->text());
        if(!s(r.smFolder->text()).isEmpty()) sc["start_menu_folder"]=s(r.smFolder->text());
        if(r.admin->isChecked()) sc["run_as_admin"]=true; scs.append(sc); }
    o["shortcuts"]=scs;

    QJsonObject regs;
    for(const auto& r:m_regRows){ if(!r.key) continue; QString k=s(r.key->text()), v=s(r.value->text()); if(!k.isEmpty()&&!v.isEmpty()) regs[k]=QJsonObject{{"",v}}; }
    o["extra_registry"]=regs;

    QJsonObject unst;
    unst["uninstall_string"]=s(m_uninstString->text()); unst["quiet_uninstall_string"]=s(m_quietUninst->text());
    unst["display_icon"]=s(m_uninstDispIcon->text()); unst["estimated_size"]=m_estSize->value();
    unst["no_modify"]=m_noModify->isChecked(); unst["no_repair"]=m_noRepair->isChecked();
    unst["help_link"]=s(m_helpLink->text()); o["uninstall"]=unst;

    QJsonObject env;
    if(!s(m_userPath->text()).isEmpty()){ QJsonObject u; u["PATH"]=s(m_userPath->text()); env["user"]=u; }
    if(!s(m_sysPath->text()).isEmpty()){ QJsonObject u; u["PATH"]=s(m_sysPath->text()); env["system"]=u; }
    if(!env.isEmpty()) o["environment"]=env;

    QJsonArray fas;
    for(const auto& r:m_assocRows){ if(!r.ext) continue;
        QJsonObject fa; fa["extension"]=s(r.ext->text()); fa["description"]=s(r.desc->text()); fa["open_command"]=s(r.cmd->text());
        if(!s(r.icon->text()).isEmpty()) fa["icon"]=s(r.icon->text());
        if(!s(r.progId->text()).isEmpty()) fa["prog_id"]=s(r.progId->text()); fas.append(fa); }
    o["file_associations"]=fas;

    QJsonObject fa;
    fa["run_program"]=m_runProg->isChecked(); fa["run_arguments"]=s(m_runArgs->text());
    fa["open_readme"]=s(m_openReadme->text()); fa["restart_required"]=m_restart->isChecked();
    o["finish_actions"]=fa;

    QJsonObject adv;
    adv["create_uninstaller"]=m_createUninst->isChecked(); adv["enable_logging"]=m_enableLog->isChecked();
    adv["silent_install"]=m_silentInst->isChecked(); adv["minimum_os_version"]=s(m_minOs->text());
    adv["64_bit_only"]=m_64bitOnly->isChecked();
    if(!s(m_preservePats->text()).isEmpty()){ QJsonArray pa; for(auto& p:s(m_preservePats->text()).split(',')){ QString t=p.trimmed(); if(!t.isEmpty()) pa.append(t); } adv["update_preserve_patterns"]=pa; }
    o["advanced"]=adv;

    return o;
}

void Configurator::applyFormData(const QJsonObject& o)
{
    if (o.isEmpty()) return;
    auto s = [](const QJsonValue& v){ return v.isString() ? v.toString() : QString(); };
    auto b = [](const QJsonValue& v, bool d){ return v.isBool() ? v.toBool() : d; };

    QJsonObject inst = o["installer"].toObject();
    if (inst.isEmpty()) return;
    m_name->setText(s(inst["name"])); m_shortName->setText(s(inst["short_name"]));
    m_version->setText(s(inst["version"])); m_publisher->setText(s(inst["publisher"]));
    m_copyright->setText(s(inst["copyright"])); m_website->setText(s(inst["website"]));
    m_email->setText(s(inst["support_email"]));
    int ri = m_rootCombo->findData(s(inst["default_install_root"])); if(ri>=0) m_rootCombo->setCurrentIndex(ri);
    m_folder->setText(s(inst["default_install_folder"])); m_exe->setText(s(inst["main_executable"]));
    m_license->setText(s(inst["license_file"]));
    m_reqAdmin->setChecked(b(inst["requires_admin"],false)); m_allowPath->setChecked(b(inst["allow_change_path"],true));
    m_icon->setText(s(inst["icon"])); m_setupIcon->setText(s(inst["setup_icon"]));
    m_uninstIcon->setText(s(inst["uninstall_icon"])); m_banner->setText(s(inst["banner_image"]));
    m_bgColor->setText(s(inst["background_color"]).isEmpty()?QStringLiteral("#FFFFFF"):s(inst["background_color"]));
    int si=m_uiStyle->findData(s(inst["ui_style"])); if(si>=0) m_uiStyle->setCurrentIndex(si);
    m_welcomeTitle->setText(s(inst["welcome_title"])); m_welcomeText->setText(s(inst["welcome_text"]));
    m_finishTitle->setText(s(inst["finish_title"])); m_finishMsg->setText(s(o["finish_message"]));

    QJsonObject fs = o["files_strategy"].toObject();
    if(!fs.isEmpty()){
        int mi=m_fileMode->findText(s(fs["mode"])); if(mi>=0) m_fileMode->setCurrentIndex(mi);
        QString ov=s(fs["overwrite"]); if(ov=="always") m_overwrite->setCurrentIndex(1); else if(ov=="never") m_overwrite->setCurrentIndex(2); else m_overwrite->setCurrentIndex(0);
        QJsonArray ex=fs["exclude"].toArray(); QStringList el; for(const auto& v:ex) el<<v.toString(); m_exclude->setText(el.join(QStringLiteral(", ")));
    }
    m_filesDir->setText(s(o["_files_source"]));

    // shortcuts — reload from saved data (clear existing, recreate)
    for(int i=m_scRows.size()-1; i>=0; i--){ m_scRows[i]={}; }
    // Clear layout
    while(m_scLayout->count()){ auto* it=m_scLayout->takeAt(0); if(it->widget()){ delete it->widget(); } else if(it->layout()){ while(it->layout()->count()){ auto* c=it->layout()->takeAt(0); delete c->widget(); delete c; } delete it->layout(); } delete it; }
    m_scRows.clear();

    QJsonArray scs=o["shortcuts"].toArray();
    for(const auto& v:scs){
        onAddShortcut();
        auto& r=m_scRows.last(); QJsonObject sc=v.toObject();
        r.target->setText(s(sc["target"]));
        int li=r.location->findText(s(sc["location"])); if(li>=0) r.location->setCurrentIndex(li);
        r.name->setText(s(sc["name"])); r.desc->setText(s(sc["description"]));
        r.args->setText(s(sc["arguments"])); r.wd->setText(s(sc["working_dir"]));
        r.icon->setText(s(sc["icon"])); r.smFolder->setText(s(sc["start_menu_folder"]));
        r.admin->setChecked(b(sc["run_as_admin"],false));
    }
    if(scs.isEmpty()){ onAddShortcut(); onAddShortcut(); }

    QJsonObject unst=o["uninstall"].toObject();
    m_uninstString->setText(s(unst["uninstall_string"])); m_quietUninst->setText(s(unst["quiet_uninstall_string"]));
    m_uninstDispIcon->setText(s(unst["display_icon"])); m_estSize->setValue(unst["estimated_size"].toInt());
    m_noModify->setChecked(b(unst["no_modify"],true)); m_noRepair->setChecked(b(unst["no_repair"],true));
    m_helpLink->setText(s(unst["help_link"]));

    QJsonObject env=o["environment"].toObject();
    m_userPath->setText(s(env["user"].toObject()["PATH"])); m_sysPath->setText(s(env["system"].toObject()["PATH"]));

    QJsonObject fa=o["finish_actions"].toObject();
    m_runProg->setChecked(b(fa["run_program"],true)); m_runArgs->setText(s(fa["run_arguments"]));
    m_openReadme->setText(s(fa["open_readme"])); m_restart->setChecked(b(fa["restart_required"],false));

    QJsonObject adv=o["advanced"].toObject();
    m_createUninst->setChecked(b(adv["create_uninstaller"],true));
    m_enableLog->setChecked(b(adv["enable_logging"],false));
    m_silentInst->setChecked(b(adv["silent_install"],false));
    m_minOs->setText(s(adv["minimum_os_version"])); m_64bitOnly->setChecked(b(adv["64_bit_only"],false));
    QJsonArray pp=adv["update_preserve_patterns"].toArray(); QStringList ppl; for(const auto& v:pp) ppl<<v.toString();
    m_preservePats->setText(ppl.join(QStringLiteral(", ")));
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Generate
// ═══════════════════════════════════════════════════════════════════════════════

bool Configurator::generatePackageJson(const QString& packageDir)
{
    QString name=m_name->text().trimmed(), ver=m_version->text().trimmed();
    QString pub=m_publisher->text().trimmed(), folder=m_folder->text().trimmed(), exe=m_exe->text().trimmed();
    if(name.isEmpty()||folder.isEmpty()||exe.isEmpty()){ QMessageBox::warning(this,tr("错误"),tr("产品名、安装文件夹和主程序不能为空。")); return false; }

    QJsonObject inst;
    inst["name"]=name; inst["version"]=ver; inst["publisher"]=pub;
    inst["default_install_root"]=m_rootCombo->currentData().toString();
    inst["default_install_folder"]=folder; inst["main_executable"]=exe;

    auto s = [](const QString& x){ return x.trimmed(); };
    if(!s(m_shortName->text()).isEmpty()) inst["short_name"]=s(m_shortName->text());
    if(!s(m_copyright->text()).isEmpty()) inst["copyright"]=s(m_copyright->text());
    if(!s(m_website->text()).isEmpty()) inst["website"]=s(m_website->text());
    if(!s(m_email->text()).isEmpty()) inst["support_email"]=s(m_email->text());
    if(!s(m_license->text()).isEmpty()){ QFileInfo fi(s(m_license->text())); inst["license_file"]=fi.fileName(); }
    if(m_reqAdmin->isChecked()) inst["requires_admin"]=true;
    if(!m_allowPath->isChecked()) inst["allow_change_path"]=false;

    // Icons
    if(!s(m_icon->text()).isEmpty()){ QFileInfo fi(s(m_icon->text())); inst["icon"]=fi.fileName(); }
    if(!s(m_setupIcon->text()).isEmpty()){ QFileInfo fi(s(m_setupIcon->text())); inst["setup_icon"]=fi.fileName(); }
    if(!s(m_uninstIcon->text()).isEmpty()){ QFileInfo fi(s(m_uninstIcon->text())); inst["uninstall_icon"]=fi.fileName(); }
    if(!s(m_banner->text()).isEmpty()){ QFileInfo fi(s(m_banner->text())); inst["banner_image"]=fi.fileName(); }
    if(!s(m_bgColor->text()).isEmpty()) inst["background_color"]=s(m_bgColor->text());
    inst["ui_style"]=m_uiStyle->currentData().toString();
    if(!s(m_welcomeTitle->text()).isEmpty()) inst["welcome_title"]=s(m_welcomeTitle->text());
    if(!s(m_welcomeText->text()).isEmpty()) inst["welcome_text"]=s(m_welcomeText->text());
    if(!s(m_finishTitle->text()).isEmpty()) inst["finish_title"]=s(m_finishTitle->text());

    QJsonObject pkg; pkg["installer"]=inst;
    pkg["finish_message"]=s(m_finishMsg->text());

    // files_strategy
    QJsonObject fs;
    fs["mode"]=m_fileMode->currentText();
    QString ovText=m_overwrite->currentText();
    if(ovText==tr("总是覆盖")) fs["overwrite"]="always"; else if(ovText==tr("跳过")) fs["overwrite"]="never"; else fs["overwrite"]="ask";
    QString ex=s(m_exclude->text());
    if(!ex.isEmpty()){ QJsonArray ea; for(const auto& p:ex.split(',')){ QString t=p.trimmed(); if(!t.isEmpty()) ea.append(t); } fs["exclude"]=ea; }
    pkg["files_strategy"]=fs;

    // shortcuts
    QJsonArray scArr;
    for(const auto& r:m_scRows){ if(!r.target) continue;
        QString t=s(r.target->text()); if(t.isEmpty()) continue;
        QJsonObject sc; sc["target"]=t; sc["location"]=r.location->currentText();
        if(!s(r.name->text()).isEmpty()) sc["name"]=s(r.name->text());
        if(!s(r.desc->text()).isEmpty()) sc["description"]=s(r.desc->text());
        if(!s(r.args->text()).isEmpty()) sc["arguments"]=s(r.args->text());
        if(!s(r.wd->text()).isEmpty()) sc["working_dir"]=s(r.wd->text());
        if(!s(r.icon->text()).isEmpty()) sc["icon"]=s(r.icon->text());
        if(!s(r.smFolder->text()).isEmpty()) sc["start_menu_folder"]=s(r.smFolder->text());
        if(r.admin->isChecked()) sc["run_as_admin"]=true;
        scArr.append(sc);
    }
    pkg["shortcuts"]=scArr;

    // registry
    QJsonObject reg;
    reg[QStringLiteral("HKEY_CURRENT_USER\\Software\\%1").arg(name)]=QJsonObject{{"Version","{VERSION}"},{"Path","{INSTALL_DIR}"}};
    for(const auto& r:m_regRows){ if(!r.key) continue; QString k=s(r.key->text()),v=s(r.value->text()); if(!k.isEmpty()&&!v.isEmpty()) reg[k]=QJsonObject{{"",v}}; }
    pkg["registry"]=reg;

    // uninstall
    QJsonObject unst;
    if(!s(m_uninstString->text()).isEmpty()) unst["uninstall_string"]=s(m_uninstString->text());
    if(!s(m_quietUninst->text()).isEmpty()) unst["quiet_uninstall_string"]=s(m_quietUninst->text());
    if(!s(m_uninstDispIcon->text()).isEmpty()) unst["display_icon"]=s(m_uninstDispIcon->text());
    if(m_estSize->value()>0) unst["estimated_size"]=m_estSize->value();
    unst["no_modify"]=m_noModify->isChecked(); unst["no_repair"]=m_noRepair->isChecked();
    if(!s(m_helpLink->text()).isEmpty()) unst["help_link"]=s(m_helpLink->text());
    pkg["uninstall"]=unst;

    // environment
    QJsonObject env;
    auto addEnv=[&](const QString& scope,QLineEdit* path,QLineEdit* vars){
        QJsonObject o;
        if(!s(path->text()).isEmpty()) o["PATH"]=s(path->text());
        if(!s(vars->text()).isEmpty()){
            for(const auto& kv:s(vars->text()).split(',')){
                int eq=kv.indexOf('='); if(eq>0) o[kv.left(eq).trimmed()]=kv.mid(eq+1).trimmed();
            }
        }
        if(!o.isEmpty()) env[scope]=o;
    };
    addEnv("user",m_userPath,m_userVars); addEnv("system",m_sysPath,m_sysVars);
    if(!env.isEmpty()) pkg["environment"]=env;

    // file_associations
    QJsonArray faArr;
    for(const auto& r:m_assocRows){ if(!r.ext) continue; QString e=s(r.ext->text()); if(e.isEmpty()) continue;
        QJsonObject fa; fa["extension"]=e; fa["description"]=s(r.desc->text()); fa["open_command"]=s(r.cmd->text());
        if(!s(r.icon->text()).isEmpty()) fa["icon"]=s(r.icon->text());
        if(!s(r.progId->text()).isEmpty()) fa["prog_id"]=s(r.progId->text());
        faArr.append(fa);
    }
    if(!faArr.isEmpty()) pkg["file_associations"]=faArr;

    // finish_actions
    QJsonObject fa;
    fa["run_program"]=m_runProg->isChecked();
    if(!s(m_runArgs->text()).isEmpty()) fa["run_arguments"]=s(m_runArgs->text());
    if(!s(m_openReadme->text()).isEmpty()) fa["open_readme"]=s(m_openReadme->text());
    if(m_restart->isChecked()) fa["restart_required"]=true;
    pkg["finish_actions"]=fa;

    // advanced
    QJsonObject adv;
    adv["create_uninstaller"]=m_createUninst->isChecked();
    if(m_enableLog->isChecked()) adv["enable_logging"]=true;
    if(m_silentInst->isChecked()) adv["silent_install"]=true;
    if(!s(m_minOs->text()).isEmpty()) adv["minimum_os_version"]=s(m_minOs->text());
    if(m_64bitOnly->isChecked()) adv["64_bit_only"]=true;
    if(!s(m_preservePats->text()).isEmpty()){
        QJsonArray pa;
        for(const auto& p:s(m_preservePats->text()).split(',')){
            QString t=p.trimmed(); if(!t.isEmpty()) pa.append(t);
        }
        adv["update_preserve_patterns"]=pa;
    }
    pkg["advanced"]=adv;

    // Write
    QDir().mkpath(packageDir);
    QFile pf(packageDir+"/package.json");
    if(!pf.open(QIODevice::WriteOnly)){ QMessageBox::warning(this,tr("错误"),tr("无法写入 package.json")); return false; }
    pf.write(QJsonDocument(pkg).toJson(QJsonDocument::Indented)); pf.close();

    // Copy files
    QString tf=packageDir+"/files"; QDir().mkpath(tf);
    QString fd=s(m_filesDir->text());
    if(!fd.isEmpty()){
        QDirIterator it(fd,QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
        while(it.hasNext()){ it.next(); QFileInfo fi=it.fileInfo(); QString rel=QDir(fd).relativeFilePath(fi.absoluteFilePath()), dest=tf+"/"+rel;
            if(fi.isDir()) QDir().mkpath(dest); else{ QDir().mkpath(QFileInfo(dest).absolutePath()); if(QFile::exists(dest)) QFile::remove(dest); QFile::copy(fi.absoluteFilePath(),dest); } }
    }
    // Copy license
    QString lic=s(m_license->text());
    if(!lic.isEmpty()){ QFileInfo lfi(lic); QString dest=tf+"/"+lfi.fileName(); if(QFile::exists(dest)) QFile::remove(dest); QFile::copy(lic,dest); }
    // Copy icons
    for(QLineEdit* e:{m_icon,m_setupIcon,m_uninstIcon,m_banner}){
        QString p=s(e->text()); if(!p.isEmpty()){ QFileInfo fi(p); QString dest=tf+"/"+fi.fileName(); if(QFile::exists(dest)) QFile::remove(dest); QFile::copy(p,dest); }
    }
    return true;
}

bool Configurator::runBuild(const QString& packageDir, const QString& outputDir)
{
    m_progressBar->setVisible(true); m_generateBtn->setEnabled(false); m_log->clear(); QApplication::processEvents();
    QString src=QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../.."));
    QString tmp=QStandardPaths::writableLocation(QStandardPaths::TempLocation)+"/InstallerMaker_build";
    QDir(tmp).removeRecursively(); QDir().mkpath(tmp);

    auto run=[this](const QString& d,const QString& p,const QStringList& a)->bool{
        QProcess proc; proc.setWorkingDirectory(d);
        QObject::connect(&proc,&QProcess::readyReadStandardOutput,this,[&](){ QString s=QString::fromLocal8Bit(proc.readAllStandardOutput()); if(!s.trimmed().isEmpty()) m_log->append(s.trimmed()); });
        QObject::connect(&proc,&QProcess::readyReadStandardError,this,[&](){ QString s=QString::fromLocal8Bit(proc.readAllStandardError()); if(!s.trimmed().isEmpty()) m_log->append(s.trimmed()); });
        QEventLoop loop; QObject::connect(&proc,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),&loop,&QEventLoop::quit);
        proc.start(p,a); if(!proc.waitForStarted(10000)){ m_log->append(tr("[错误] 无法启动: %1").arg(p)); return false; }
        loop.exec(); return proc.exitCode()==0;
    };

    m_log->append(tr("═══ 配置 CMake... ═══")); QApplication::processEvents();
    if(!run(tmp,QStringLiteral("cmake"),{QStringLiteral("-G"),QStringLiteral("Ninja"),QStringLiteral("-DCMAKE_BUILD_TYPE=Release"),QStringLiteral("-DPACKAGE_DIR=%1").arg(packageDir),src})) goto fail;

    m_log->append(tr("═══ 编译... ═══")); QApplication::processEvents();
    if(!run(tmp,QStringLiteral("cmake"),{QStringLiteral("--build"),QStringLiteral("."),QStringLiteral("--config"),QStringLiteral("Release")})) goto fail;

    { QString s=tmp+"/Setup.exe",d=outputDir+"/Setup.exe"; if(QFile::exists(s)){ if(QFile::exists(d)) QFile::remove(d); QFile::copy(s,d); m_log->append(tr("═══ 完成! ═══\n%1").arg(QDir::toNativeSeparators(d))); }
      else{ m_log->append(tr("[错误] 未找到 Setup.exe")); goto fail; } }
    QDir(tmp).removeRecursively();
    m_progressBar->setVisible(false); m_generateBtn->setEnabled(true);
    QMessageBox::information(this,tr("成功"),tr("安装程序已生成:\n%1").arg(QDir::toNativeSeparators(outputDir+"/Setup.exe")));
    return true;
fail:
    m_progressBar->setVisible(false); m_generateBtn->setEnabled(true);
    QMessageBox::critical(this,tr("构建失败"),tr("构建失败，请检查日志。")); return false;
}

void Configurator::onGenerate()
{
    QString out=m_output->text().trimmed(), pkg=out+"/package";
    if(out.isEmpty()){ QMessageBox::warning(this,tr("错误"),tr("请选择输出目录。")); return; }
    if(!generatePackageJson(pkg)) return;
    runBuild(pkg,out);
}
