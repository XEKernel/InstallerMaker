#include "installwizard.hpp"
#include "installerengine.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QStackedWidget>
#include <QTextEdit>
#include <QThread>
#include <QVBoxLayout>

// ═══════════════════════════════════════════════════════════════════════════════
//  Constructor
// ═══════════════════════════════════════════════════════════════════════════════

InstallWizard::InstallWizard(Mode mode, const QJsonObject& package,
                             const QString& uninstallDir, QWidget* parent)
    : QDialog(parent), m_mode(mode), m_package(package), m_uninstallDir(uninstallDir)
{
    m_installSteps   = { tr("欢迎"), tr("许可协议"), tr("安装位置"), tr("安装进度"), tr("完成") };
    m_uninstallSteps = { tr("确认卸载"), tr("卸载进度"), tr("完成") };

    QString uiStyle = package.value(QStringLiteral("installer")).toObject()
                          .value(QStringLiteral("ui_style")).toString(QStringLiteral("wizard"));
    m_style = (uiStyle == QStringLiteral("oneclick") && mode == Install) ? OneClick : Wizard;

    if (m_style == OneClick)
        { setupOneClickUi(); return; }

    setupWizardUi();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  One-click UI
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::setupOneClickUi()
{
    const auto inst = m_package.value(QStringLiteral("installer")).toObject();
    const QString name    = inst.value(QStringLiteral("name")).toString();
    const QString ver     = inst.value(QStringLiteral("version")).toString();
    const QString pub     = inst.value(QStringLiteral("publisher")).toString();
    const QString defRoot = InstallerEngine::resolvePath(inst.value(QStringLiteral("default_install_root")).toString());
    const QString defFolder = inst.value(QStringLiteral("default_install_folder")).toString();

    setWindowTitle(tr("%1 安装程序").arg(name));
    setMinimumSize(480, 500); resize(520, 540);

    auto* main = new QVBoxLayout(this);
    main->setContentsMargins(30, 24, 30, 20); main->setSpacing(12);

    auto* title = new QLabel(tr("<h2>%1</h2>").arg(name), this);
    title->setAlignment(Qt::AlignCenter); main->addWidget(title);

    auto* verInfo = new QLabel(tr("版本 %1  |  %2").arg(ver, pub), this);
    verInfo->setAlignment(Qt::AlignCenter); main->addWidget(verInfo);

    auto* sep = new QFrame(this); sep->setFrameShape(QFrame::HLine); main->addWidget(sep);

    // Install path
    auto* pathRow = new QHBoxLayout();
    m_ocPathEdit = new QLineEdit(QDir::toNativeSeparators(QDir(defRoot).absoluteFilePath(defFolder)), this);
    pathRow->addWidget(m_ocPathEdit, 1);
    auto* browse = new QPushButton(tr("浏览..."), this);
    connect(browse, &QPushButton::clicked, this, [this]() {
        QString d = QFileDialog::getExistingDirectory(this, tr("选择安装目录"), m_ocPathEdit->text());
        if (!d.isEmpty()) m_ocPathEdit->setText(QDir::toNativeSeparators(d));
    });
    pathRow->addWidget(browse);
    main->addLayout(pathRow);

    // Options
    m_ocDesktopBox   = new QCheckBox(tr("创建桌面快捷方式"), this);
    m_ocDesktopBox->setChecked(true); main->addWidget(m_ocDesktopBox);

    m_ocStartMenuBox = new QCheckBox(tr("创建开始菜单快捷方式"), this);
    m_ocStartMenuBox->setChecked(true); main->addWidget(m_ocStartMenuBox);

    m_ocAutoStartBox = new QCheckBox(tr("开机自启"), this);
    main->addWidget(m_ocAutoStartBox);

    // License
    auto* licRow = new QHBoxLayout();
    m_ocLicenseBox = new QCheckBox(tr("我已阅读并同意"), this);
    licRow->addWidget(m_ocLicenseBox);
    m_ocLicenseBtn = new QPushButton(tr("许可协议"), this);
    m_ocLicenseBtn->setFlat(true);
    m_ocLicenseBtn->setStyleSheet(QStringLiteral("color:#0066cc; text-decoration:underline; border:none;"));
    connect(m_ocLicenseBtn, &QPushButton::clicked, this, &InstallWizard::onShowLicense);
    licRow->addWidget(m_ocLicenseBtn); licRow->addStretch();
    main->addLayout(licRow);

    // Install button
    auto* sep2 = new QFrame(this); sep2->setFrameShape(QFrame::HLine); main->addWidget(sep2);

    m_ocInstallBtn = new QPushButton(tr("立即安装"), this);
    m_ocInstallBtn->setMinimumHeight(44);
    m_ocInstallBtn->setStyleSheet(QStringLiteral("QPushButton{font-size:14pt;font-weight:bold;padding:8px 32px}"));
    m_ocInstallBtn->setEnabled(false);
    connect(m_ocInstallBtn, &QPushButton::clicked, this, &InstallWizard::onOneClickInstall);
    connect(m_ocLicenseBox, &QCheckBox::toggled, m_ocInstallBtn, &QPushButton::setEnabled);

    auto* btnRow = new QHBoxLayout(); btnRow->addStretch();
    btnRow->addWidget(m_ocInstallBtn); btnRow->addStretch();
    main->addLayout(btnRow);

    // Progress (hidden)
    m_ocProgressBar = new QProgressBar(this); m_ocProgressBar->setRange(0,100);
    m_ocProgressBar->setValue(0); m_ocProgressBar->setVisible(false);
    main->addWidget(m_ocProgressBar);

    m_ocStatusLabel = new QLabel(this); m_ocStatusLabel->setWordWrap(true);
    m_ocStatusLabel->setVisible(false);
    main->addWidget(m_ocStatusLabel);

    main->addStretch();
}

void InstallWizard::onShowLicense()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("许可协议")); dlg->resize(500, 400);
    auto* lay = new QVBoxLayout(dlg);
    auto* edit = new QTextEdit(dlg); edit->setReadOnly(true);
    QString lf = m_package.value(QStringLiteral("installer")).toObject()
                     .value(QStringLiteral("license_file")).toString();
    if (!lf.isEmpty()) {
        QFile f(QStringLiteral(":/package/files/%1").arg(lf));
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            edit->setPlainText(QString::fromUtf8(f.readAll()));
    } else { edit->setPlainText(tr("(未提供许可协议)")); }
    lay->addWidget(edit);
    auto* close = new QPushButton(tr("关闭"), dlg);
    connect(close, &QPushButton::clicked, dlg, &QDialog::accept);
    lay->addWidget(close); dlg->exec(); dlg->deleteLater();
}

void InstallWizard::onOneClickInstall()
{
    m_ocPathEdit->setEnabled(false);      m_ocDesktopBox->setEnabled(false);
    m_ocStartMenuBox->setEnabled(false);  m_ocAutoStartBox->setEnabled(false);
    m_ocLicenseBox->setEnabled(false);    m_ocLicenseBtn->setEnabled(false);
    m_ocInstallBtn->setVisible(false);

    m_ocProgressBar->setVisible(true); m_ocStatusLabel->setVisible(true);
    startInstallation(m_ocPathEdit->text());
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Wizard UI
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::setupWizardUi()
{
    const QString appName = m_package.value(QStringLiteral("installer")).toObject()
                                .value(QStringLiteral("name")).toString();
    setWindowTitle((m_mode == Install) ? tr("%1 安装程序").arg(appName)
                                       : tr("%1 卸载程序").arg(appName));
    setMinimumSize(640, 440); resize(700, 480);

    auto* ml = new QHBoxLayout(this); ml->setContentsMargins(0,0,0,0); ml->setSpacing(0);
    auto* lp = new QFrame(this); lp->setFixedWidth(180); lp->setFrameShape(QFrame::NoFrame);
    { auto pal=lp->palette(); pal.setColor(QPalette::Window,pal.color(QPalette::Window).darker(108));
      lp->setPalette(pal); lp->setAutoFillBackground(true); }
    auto* ll = new QVBoxLayout(lp); ll->setContentsMargins(20,30,20,20); ll->setSpacing(4);
    m_stepLayout = new QVBoxLayout; m_stepLayout->setSpacing(4);
    ll->addLayout(m_stepLayout); ll->addStretch(); ml->addWidget(lp);

    auto* rp = new QFrame(this); rp->setFrameShape(QFrame::NoFrame);
    auto* rl = new QVBoxLayout(rp); rl->setContentsMargins(24,20,24,16);
    m_stack = new QStackedWidget(this); rl->addWidget(m_stack,1);
    auto* br = new QHBoxLayout; br->addStretch();
    m_backBtn=new QPushButton(tr("< 上一步(&B)"),this); m_nextBtn=new QPushButton(tr("下一步(&N) >"),this);
    m_cancelBtn=new QPushButton(tr("取消"),this);
    m_backBtn->setMinimumWidth(100); m_nextBtn->setMinimumWidth(100); m_cancelBtn->setMinimumWidth(80);
    br->addWidget(m_backBtn); br->addWidget(m_nextBtn); br->addWidget(m_cancelBtn);
    rl->addLayout(br); ml->addWidget(rp,1);

    connect(m_backBtn,&QPushButton::clicked,this,&InstallWizard::onBack);
    connect(m_nextBtn,&QPushButton::clicked,this,&InstallWizard::onNext);
    connect(m_cancelBtn,&QPushButton::clicked,this,&QDialog::reject);

    if(m_mode==Install){
        m_stack->addWidget(buildWelcomePage());
        m_stack->addWidget(buildLicensePage());
        m_stack->addWidget(buildPathPage());
        m_stack->addWidget(buildProgressPage());
        m_stack->addWidget(buildFinishPage());
    } else {
        m_stack->addWidget(buildUninstallConfirmPage());
        m_stack->addWidget(buildUninstallProgressPage());
        m_stack->addWidget(buildUninstallFinishPage());
    }

    refreshStepIndicator(); navigateTo(0);
}

// ... (wizard page builders — same as before, kept below)
// (including buildWelcomePage, buildLicensePage, buildPathPage, buildProgressPage,
//  buildFinishPage, buildUninstallConfirmPage, buildUninstallProgressPage,
//  buildUninstallFinishPage, navigation, progress callbacks, done override, etc.)

// NOTE: The full wizard implementation continues here. Due to the large file,
// the remaining wizard methods (pages + navigation + slots) are unchanged
// from the previous version. The ONLY additions are setupOneClickUi(),
// onShowLicense(), and onOneClickInstall() above.
// 
// The startInstallation(), done(), onProgressChanged(), onStatusChanged(),
// and onFinished() methods are shared between both modes.
// 
// In one-click mode, progress updates go to m_ocProgressBar/m_ocStatusLabel
// instead of m_progressBar/m_statusLabel. In wizard mode they use the original
// widgets. The signal handlers (onProgressChanged/onStatusChanged/onFinished)
// need to check which mode is active.

// ═══════════════════════════════════════════════════════════════════════════════
//  Shared: startInstallation
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::startInstallation(const QString& installDir)
{
    const bool forceAdmin = m_package.value(QStringLiteral("installer")).toObject()
                                .value(QStringLiteral("requires_admin")).toBool(false);
    const bool needsAdmin = forceAdmin || !InstallerEngine::canWriteToDir(installDir);

    if (needsAdmin) {
        bool ok = InstallerEngine::elevateSelf(installDir, m_package);
        if (ok) {
            if (m_style == OneClick) {
                m_ocStatusLabel->setText(tr("安装完成！"));
                m_ocProgressBar->setValue(100);
                QMessageBox::information(this, tr("完成"),
                    m_package.value(QStringLiteral("finish_message")).toString());
                accept();
            } else {
                m_statusLabel->setText(tr("安装完成！"));
                m_progressBar->setValue(100);
                navigateTo(4);
            }
        } else {
            QMessageBox::critical(this, tr("错误"),
                tr("需要管理员权限但无法提升。"));
        }
        return;
    }

    if (m_style == Wizard) navigateTo(3);

    auto* engine = new InstallerEngine(); auto* thread = new QThread(this);
    engine->loadPackage(); engine->moveToThread(thread);

    connect(engine, &InstallerEngine::progressChanged, this, &InstallWizard::onProgressChanged);
    connect(engine, &InstallerEngine::statusChanged,   this, &InstallWizard::onStatusChanged);
    connect(engine, &InstallerEngine::finished,         this, &InstallWizard::onFinished);

    connect(thread, &QThread::started, engine, [engine, installDir]() { engine->install(installDir); });
    connect(engine, &InstallerEngine::finished, thread, [thread, engine]() { engine->deleteLater(); thread->quit(); });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Shared: progress callbacks
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::onProgressChanged(int percent)
{
    if (m_style == OneClick) { if (m_ocProgressBar) m_ocProgressBar->setValue(percent); }
    else { if (m_progressBar) m_progressBar->setValue(percent); }
}

void InstallWizard::onStatusChanged(const QString& text)
{
    if (m_style == OneClick) { if (m_ocStatusLabel) m_ocStatusLabel->setText(text); }
    else { if (m_statusLabel) m_statusLabel->setText(text); }
}

void InstallWizard::onFinished(bool success, const QString& error)
{
    if (!success) {
        QMessageBox::critical(this, tr("安装失败"), error);
        return;
    }
    if (m_style == OneClick) {
        QMessageBox::information(this, tr("完成"),
            m_package.value(QStringLiteral("finish_message")).toString());
        accept();
    } else {
        auto* pp = dynamic_cast<QWidget*>(m_stack->widget(3)); Q_UNUSED(pp);
        if (m_progressBar) m_progressBar->setValue(100);
        if (m_statusLabel) m_statusLabel->setText(tr("安装完成！"));
        if (m_backBtn) m_backBtn->setVisible(false);
        if (m_cancelBtn) m_cancelBtn->setVisible(false);
        m_nextBtn->setText(tr("完成(&F)")); m_nextBtn->setVisible(true);
        m_nextBtn->setEnabled(true);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Shared: done
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::done(int result)
{
    if (result == QDialog::Accepted && m_mode == Install && m_runCheckBox
        && m_runCheckBox->isChecked()) {
        QString dir = (m_style == OneClick) ? m_ocPathEdit->text() : m_pathEdit->text();
        QString exe = QDir(dir).absoluteFilePath(
            m_package.value(QStringLiteral("installer")).toObject()
                .value(QStringLiteral("main_executable")).toString());
        if (QFile::exists(exe)) QProcess::startDetached(exe, {}, dir);
    }
    QDialog::done(result);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Wizard: page builders
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* InstallWizard::buildWelcomePage()
{
    const auto inst = m_package.value(QStringLiteral("installer")).toObject();
    const QString name = inst.value(QStringLiteral("name")).toString();
    const QString ver  = inst.value(QStringLiteral("version")).toString();
    const QString pub  = inst.value(QStringLiteral("publisher")).toString();
    const QString copy = inst.value(QStringLiteral("copyright")).toString();
    const QString wt   = inst.value(QStringLiteral("welcome_title")).toString();
    const QString wx   = inst.value(QStringLiteral("welcome_text")).toString();

    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p);
    l->setContentsMargins(0,10,0,10);
    auto* t = new QLabel(wt.isEmpty()?tr("<h2>欢迎使用 %1 安装向导</h2>").arg(name)
                                     :QStringLiteral("<h2>%1</h2>").arg(wt), p);
    t->setWordWrap(true); l->addWidget(t);
    auto* i = new QLabel(wx.isEmpty()?tr("<p>本向导将引导您完成 <b>%1</b> 的安装。</p>").arg(name):wx, p);
    i->setWordWrap(true); l->addWidget(i);
    auto* vi = new QLabel(tr("<p>版本: %1 &nbsp;|&nbsp; 发布者: %2</p>").arg(ver, pub), p);
    vi->setWordWrap(true); l->addWidget(vi);
    if(!copy.isEmpty()){ auto* c=new QLabel(copy,p); c->setWordWrap(true); l->addWidget(c); }
    l->addStretch(); return p;
}

QWidget* InstallWizard::buildLicensePage()
{
    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p);
    l->setContentsMargins(0,10,0,10);
    l->addWidget(new QLabel(tr("<b>请阅读以下许可协议：</b>"), p));
    auto* e = new QTextEdit(p); e->setReadOnly(true);
    QString lf = m_package.value(QStringLiteral("installer")).toObject()
                     .value(QStringLiteral("license_file")).toString();
    if(!lf.isEmpty()){ QFile f(QStringLiteral(":/package/files/%1").arg(lf));
        if(f.open(QIODevice::ReadOnly|QIODevice::Text)) e->setPlainText(QString::fromUtf8(f.readAll())); }
    l->addWidget(e,1);
    m_licenseBox = new QCheckBox(tr("我接受许可协议的条款(&A)"), p);
    l->addWidget(m_licenseBox);
    connect(m_licenseBox, &QCheckBox::toggled, this, [this](){ refreshButtons(); });
    return p;
}

QWidget* InstallWizard::buildPathPage()
{
    const auto inst = m_package.value(QStringLiteral("installer")).toObject();
    QString def = QDir::toNativeSeparators(QDir(InstallerEngine::resolvePath(
        inst.value(QStringLiteral("default_install_root")).toString()))
        .absoluteFilePath(inst.value(QStringLiteral("default_install_folder")).toString()));

    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p);
    l->setContentsMargins(0,10,0,10);
    l->addWidget(new QLabel(tr("<b>选择安装位置</b>"), p));
    l->addSpacing(8);
    l->addWidget(new QLabel(tr("安装目录(&D):"), p));
    auto* r = new QHBoxLayout();
    m_pathEdit = new QLineEdit(def, p); r->addWidget(m_pathEdit,1);
    auto* b = new QPushButton(tr("浏览(&B)..."), p);
    connect(b,&QPushButton::clicked,this,[this](){ QString d=QFileDialog::getExistingDirectory(this,tr("选择安装目录"),m_pathEdit->text()); if(!d.isEmpty()) m_pathEdit->setText(QDir::toNativeSeparators(d)); });
    r->addWidget(b); l->addLayout(r); l->addStretch(); return p;
}

QWidget* InstallWizard::buildProgressPage()
{
    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p);
    l->setContentsMargins(0,10,0,10);
    l->addWidget(new QLabel(tr("<b>正在安装...</b>"), p));
    m_statusLabel = new QLabel(tr("正在准备..."), p); m_statusLabel->setWordWrap(true);
    l->addWidget(m_statusLabel); l->addSpacing(12);
    m_progressBar = new QProgressBar(p); m_progressBar->setRange(0,100); m_progressBar->setValue(0);
    l->addWidget(m_progressBar); l->addStretch(); return p;
}

QWidget* InstallWizard::buildFinishPage()
{
    const auto inst = m_package.value(QStringLiteral("installer")).toObject();
    const QString name = inst.value(QStringLiteral("name")).toString();
    const auto fa = m_package.value(QStringLiteral("finish_actions")).toObject();
    const bool runDef = fa.value(QStringLiteral("run_program")).toBool(true);
    const bool restart = fa.value(QStringLiteral("restart_required")).toBool(false);

    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p);
    l->setContentsMargins(0,10,0,10);
    auto* t = new QLabel(QStringLiteral("<h2>%1</h2>").arg(
        inst.value(QStringLiteral("finish_title")).toString().isEmpty()
            ? tr("安装完成") : inst.value(QStringLiteral("finish_title")).toString()), p);
    l->addWidget(t);
    QString fm = m_package.value(QStringLiteral("finish_message")).toString();
    if(fm.isEmpty()) fm=tr("%1 已成功安装。").arg(name);
    fm.replace(QStringLiteral("{name}"), name);
    auto* msg = new QLabel(fm, p); msg->setWordWrap(true); l->addWidget(msg);
    if(restart){ auto* rh=new QLabel(tr("<p><b>注意：</b>建议重新启动计算机。</p>"),p); rh->setWordWrap(true); l->addWidget(rh); }
    l->addSpacing(16);
    m_runCheckBox = new QCheckBox(tr("运行 %1(&R)").arg(name), p);
    m_runCheckBox->setChecked(runDef); l->addWidget(m_runCheckBox);
    l->addStretch(); return p;
}

// Uninstall pages (simple, same as before)
QWidget* InstallWizard::buildUninstallConfirmPage()
{
    const QString name = m_package.value(QStringLiteral("installer")).toObject()
                             .value(QStringLiteral("name")).toString();
    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p);
    l->setContentsMargins(0,10,0,10);
    l->addWidget(new QLabel(tr("<h2>卸载 %1</h2>").arg(name), p));
    l->addWidget(new QLabel(tr("<p>确定要卸载 <b>%1</b>？</p><p>安装目录: %2</p>")
        .arg(name, QDir::toNativeSeparators(m_uninstallDir)), p));
    l->addStretch(); return p;
}

QWidget* InstallWizard::buildUninstallProgressPage()
{
    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p);
    l->setContentsMargins(0,10,0,10);
    l->addWidget(new QLabel(tr("<b>正在卸载...</b>"), p));
    m_statusLabel = new QLabel(tr("正在准备..."), p); m_statusLabel->setWordWrap(true);
    l->addWidget(m_statusLabel); l->addSpacing(12);
    m_progressBar = new QProgressBar(p); m_progressBar->setRange(0,100); m_progressBar->setValue(0);
    l->addWidget(m_progressBar); l->addStretch(); return p;
}

QWidget* InstallWizard::buildUninstallFinishPage()
{
    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p);
    l->setContentsMargins(0,10,0,10);
    l->addWidget(new QLabel(tr("<h2>卸载完成</h2>"), p));
    l->addWidget(new QLabel(tr("程序已从您的计算机中移除。"), p));
    l->addStretch(); return p;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Wizard: navigation
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::navigateTo(int step)
{
    if(step<0||step>=m_stack->count()) return;
    m_currentStep=step; m_stack->setCurrentIndex(step);
    refreshStepIndicator(); refreshButtons();
}

void InstallWizard::onBack() { if(m_currentStep>0) navigateTo(m_currentStep-1); }

void InstallWizard::onNext()
{
    if(m_currentStep>=m_stack->count()-1){ accept(); return; }
    if(m_mode==Install&&m_currentStep==2){ startInstallation(m_pathEdit->text()); return; }
    if(m_mode==Uninstall&&m_currentStep==0){
        auto* e=new InstallerEngine; auto* t=new QThread(this);
        e->loadPackage(); e->moveToThread(t);
        connect(e,&InstallerEngine::progressChanged,this,&InstallWizard::onProgressChanged);
        connect(e,&InstallerEngine::statusChanged,this,&InstallWizard::onStatusChanged);
        connect(e,&InstallerEngine::finished,this,&InstallWizard::onFinished);
        navigateTo(1);
        QString d=m_uninstallDir;
        connect(t,&QThread::started,e,[e,d](){ e->uninstall(d); });
        connect(e,&InstallerEngine::finished,t,[t,e](){ e->deleteLater(); t->quit(); });
        connect(t,&QThread::finished,t,&QObject::deleteLater);
        t->start();
        return;
    }
    navigateTo(m_currentStep+1);
}

void InstallWizard::refreshButtons()
{
    int last=m_stack->count()-1;
    bool first=(m_currentStep==0), lastP=(m_currentStep>=last);
    bool prog=(m_mode==Install&&m_currentStep==3)||(m_mode==Uninstall&&m_currentStep==1);
    m_backBtn->setVisible(!first&&!prog);
    m_nextBtn->setEnabled(true);
    if(prog){ m_nextBtn->setVisible(false); }
    else if(lastP){ m_nextBtn->setText(tr("完成(&F)")); m_nextBtn->setVisible(true); }
    else if((m_mode==Install&&m_currentStep==2)||(m_mode==Uninstall&&m_currentStep==0)){
        m_nextBtn->setText((m_mode==Install)?tr("安装(&I)"):tr("卸载(&U)")); m_nextBtn->setVisible(true); }
    else { m_nextBtn->setText(tr("下一步(&N) >")); m_nextBtn->setVisible(true); }
    if(m_mode==Install&&m_currentStep==1&&m_licenseBox) m_nextBtn->setEnabled(m_licenseBox->isChecked());
    m_cancelBtn->setVisible(!lastP&&!prog);
}

void InstallWizard::refreshStepIndicator()
{
    QLayoutItem* c;
    while((c=m_stepLayout->takeAt(0))!=nullptr){ delete c->widget(); delete c; }
    const QStringList& steps=(m_mode==Install)?m_installSteps:m_uninstallSteps;
    for(int i=0;i<steps.size();i++){
        auto* row=new QHBoxLayout; row->setSpacing(10);
        auto* icon=new QLabel(this); icon->setFixedWidth(24); icon->setAlignment(Qt::AlignCenter);
        QFont f=icon->font(); f.setPointSize(12);
        if(i<m_currentStep){ icon->setText(QStringLiteral("\u2713")); f.setBold(true); }
        else if(i==m_currentStep){ icon->setText(QStringLiteral("\u25CF")); f.setBold(true); }
        else icon->setText(QStringLiteral("\u25CB"));
        icon->setFont(f); row->addWidget(icon);
        auto* label=new QLabel(steps[i],this);
        if(i==m_currentStep){ QFont lf=label->font(); lf.setBold(true); label->setFont(lf); }
        row->addWidget(label,1); row->addStretch();
        m_stepLayout->addLayout(row);
    }
}
