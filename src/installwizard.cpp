#include "installwizard.hpp"
#include "installerengine.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGroupBox>
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
InstallWizard::InstallWizard(Mode mode, const QJsonObject& package,
                             const QString& uninstallDir, QWidget* parent)
    : QDialog(parent), m_mode(mode), m_package(package), m_uninstallDir(uninstallDir)
{
    m_installSteps   = { tr("欢迎"), tr("许可协议"), tr("安装位置"), tr("安装进度"), tr("完成") };
    m_uninstallSteps = { tr("确认卸载"), tr("卸载进度"), tr("完成") };

    QString uiStyle = package.value(QStringLiteral("installer")).toObject()
                          .value(QStringLiteral("ui_style")).toString(QStringLiteral("wizard"));
    m_style = (uiStyle == QStringLiteral("oneclick") && mode == Install) ? OneClick : Wizard;

    if (m_style == OneClick) { setupOneClickUi(); return; }
    setupWizardUi();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  One-click UI — horizontal layout
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
    setMinimumSize(640, 400); resize(700, 440);

    // ── Main horizontal layout ────────────────────────────────────────────
    auto* h = new QHBoxLayout(this);
    h->setContentsMargins(24, 20, 24, 16); h->setSpacing(24);

    // Left: info + options
    auto* left = new QVBoxLayout(); left->setSpacing(10);

    auto* title = new QLabel(tr("<h3>%1 %2</h3>").arg(name, ver), this);
    left->addWidget(title);
    auto* pubLabel = new QLabel(pub, this);
    pubLabel->setStyleSheet(QStringLiteral("color:#888"));
    left->addWidget(pubLabel);

    auto* sep = new QFrame(this); sep->setFrameShape(QFrame::HLine); left->addWidget(sep);

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
    left->addWidget(new QLabel(tr("安装目录:"), this));
    left->addLayout(pathRow);

    // Options group
    auto* optGrp = new QGroupBox(tr("安装选项"), this);
    auto* optLay = new QVBoxLayout(optGrp);
    m_ocDesktopBox   = new QCheckBox(tr("创建桌面快捷方式"), this); m_ocDesktopBox->setChecked(true);
    m_ocStartMenuBox = new QCheckBox(tr("创建开始菜单快捷方式"), this); m_ocStartMenuBox->setChecked(true);
    m_ocAutoStartBox = new QCheckBox(tr("开机自启"), this);
    optLay->addWidget(m_ocDesktopBox);
    optLay->addWidget(m_ocStartMenuBox);
    optLay->addWidget(m_ocAutoStartBox);
    left->addWidget(optGrp);

    // License row
    auto* licRow = new QHBoxLayout();
    m_ocLicenseBox = new QCheckBox(tr("我已阅读并同意"), this);
    licRow->addWidget(m_ocLicenseBox);
    m_ocLicenseBtn = new QPushButton(tr("许可协议"), this);
    m_ocLicenseBtn->setFlat(true);
    m_ocLicenseBtn->setStyleSheet(QStringLiteral("color:#0066cc; text-decoration:underline; border:none;"));
    connect(m_ocLicenseBtn, &QPushButton::clicked, this, &InstallWizard::onShowLicense);
    licRow->addWidget(m_ocLicenseBtn); licRow->addStretch();
    left->addLayout(licRow);
    left->addStretch();

    // Right: install button + progress
    auto* right = new QVBoxLayout(); right->setSpacing(12);
    right->addStretch();

    // Big install button
    m_ocInstallBtn = new QPushButton(tr("立即安装"), this);
    m_ocInstallBtn->setMinimumSize(160, 120);
    m_ocInstallBtn->setStyleSheet(QStringLiteral(
        "QPushButton{font-size:16pt;font-weight:bold;padding:20px 40px;border-radius:8px;}"));
    m_ocInstallBtn->setEnabled(false);
    connect(m_ocInstallBtn, &QPushButton::clicked, this, &InstallWizard::onOneClickInstall);
    connect(m_ocLicenseBox, &QCheckBox::toggled, m_ocInstallBtn, &QPushButton::setEnabled);

    auto* btnCenter = new QHBoxLayout(); btnCenter->addStretch();
    btnCenter->addWidget(m_ocInstallBtn); btnCenter->addStretch();
    right->addLayout(btnCenter);

    // Progress
    m_ocProgressBar = new QProgressBar(this); m_ocProgressBar->setRange(0,100);
    m_ocProgressBar->setValue(0); m_ocProgressBar->setVisible(false);
    m_ocProgressBar->setMinimumWidth(200);
    right->addWidget(m_ocProgressBar);

    m_ocStatusLabel = new QLabel(this); m_ocStatusLabel->setWordWrap(true);
    m_ocStatusLabel->setVisible(false);
    right->addWidget(m_ocStatusLabel);

    right->addStretch();

    h->addLayout(left, 3);
    h->addLayout(right, 2);
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
    const auto inst = m_package.value(QStringLiteral("installer")).toObject();
    const QString appName = inst.value(QStringLiteral("name")).toString();
    setWindowTitle((m_mode == Install) ? tr("%1 安装程序").arg(appName)
                                       : tr("%1 卸载程序").arg(appName));
    setMinimumSize(680, 480); resize(740, 520);

    auto* ml = new QHBoxLayout(this); ml->setContentsMargins(0,0,0,0); ml->setSpacing(0);

    // ── Left step panel ───────────────────────────────────────────────────
    auto* lp = new QFrame(this);
    lp->setFixedWidth(170); lp->setFrameShape(QFrame::NoFrame);
    {
        auto pal = lp->palette();
        pal.setColor(QPalette::Window, QColor(45, 45, 48));
        lp->setPalette(pal); lp->setAutoFillBackground(true);
    }
    auto* ll = new QVBoxLayout(lp);
    ll->setContentsMargins(16, 24, 16, 20); ll->setSpacing(6);

    auto* header = new QLabel(tr("安装步骤"), lp);
    header->setStyleSheet(QStringLiteral("color:#aaa;font-size:11px;"));
    ll->addWidget(header);

    auto* sep = new QFrame(lp); sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet(QStringLiteral("color:#555;"));
    ll->addWidget(sep); ll->addSpacing(8);

    m_stepLayout = new QVBoxLayout; m_stepLayout->setSpacing(2);
    ll->addLayout(m_stepLayout); ll->addStretch();
    ml->addWidget(lp);

    // ── Right content area ────────────────────────────────────────────────
    auto* rp = new QFrame(this); rp->setFrameShape(QFrame::NoFrame);
    auto* rl = new QVBoxLayout(rp);
    rl->setContentsMargins(28, 24, 28, 16); rl->setSpacing(0);

    m_stack = new QStackedWidget(this);
    rl->addWidget(m_stack, 1);

    // Separator line above buttons
    auto* btnSep = new QFrame(this); btnSep->setFrameShape(QFrame::HLine);
    rl->addWidget(btnSep);
    rl->addSpacing(8);

    // Button bar
    auto* br = new QHBoxLayout; br->setSpacing(8); br->addStretch();
    m_backBtn   = new QPushButton(tr("< 上一步(&B)"), this);
    m_nextBtn   = new QPushButton(tr("下一步(&N) >"), this);
    m_cancelBtn = new QPushButton(tr("取消"), this);
    m_backBtn->setMinimumWidth(110); m_nextBtn->setMinimumWidth(110); m_cancelBtn->setMinimumWidth(80);
    m_nextBtn->setDefault(true);
    br->addWidget(m_backBtn); br->addWidget(m_nextBtn); br->addWidget(m_cancelBtn);
    rl->addLayout(br);

    ml->addWidget(rp, 1);

    connect(m_backBtn,   &QPushButton::clicked, this, &InstallWizard::onBack);
    connect(m_nextBtn,   &QPushButton::clicked, this, &InstallWizard::onNext);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    if (m_mode == Install) {
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

// ═══════════════════════════════════════════════════════════════════════════════
//  Wizard page builders
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
    l->setContentsMargins(0, 0, 0, 0); l->setSpacing(12);

    auto* t = new QLabel(wt.isEmpty()
        ? tr("<h3>欢迎使用 %1 安装向导</h3>").arg(name)
        : QStringLiteral("<h3>%1</h3>").arg(wt), p);
    t->setWordWrap(true); l->addWidget(t);

    auto* info = new QLabel(wx.isEmpty()
        ? tr("<p>本向导将引导您完成 <b>%1</b> 的安装。</p>").arg(name) : wx, p);
    info->setWordWrap(true); l->addWidget(info);

    auto* meta = new QLabel(
        tr("<p>版本: %1 &nbsp;|&nbsp; 发布者: %2</p>").arg(ver, pub), p);
    l->addWidget(meta);

    if (!copy.isEmpty()) {
        auto* c = new QLabel(copy, p); c->setWordWrap(true); c->setStyleSheet(QStringLiteral("color:#888;"));
        l->addWidget(c);
    }
    l->addStretch(); return p;
}

QWidget* InstallWizard::buildLicensePage()
{
    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p);
    l->setContentsMargins(0, 0, 0, 0);
    l->addWidget(new QLabel(tr("<b>请阅读以下许可协议：</b>"), p));

    auto* e = new QTextEdit(p); e->setReadOnly(true);
    e->setStyleSheet(QStringLiteral("QTextEdit{border:1px solid #555;border-radius:4px;padding:8px;}"));
    QString lf = m_package.value(QStringLiteral("installer")).toObject()
                     .value(QStringLiteral("license_file")).toString();
    if (!lf.isEmpty()) { QFile f(QStringLiteral(":/package/files/%1").arg(lf));
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            e->setPlainText(QString::fromUtf8(f.readAll())); }
    l->addWidget(e, 1);

    m_licenseBox = new QCheckBox(tr("我接受许可协议的条款(&A)"), p);
    l->addWidget(m_licenseBox);
    connect(m_licenseBox, &QCheckBox::toggled, this, [this]() { refreshButtons(); });
    return p;
}

QWidget* InstallWizard::buildPathPage()
{
    const auto inst = m_package.value(QStringLiteral("installer")).toObject();
    QString def = QDir::toNativeSeparators(QDir(InstallerEngine::resolvePath(
        inst.value(QStringLiteral("default_install_root")).toString()))
        .absoluteFilePath(inst.value(QStringLiteral("default_install_folder")).toString()));

    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p);
    l->setContentsMargins(0, 0, 0, 0); l->setSpacing(10);
    l->addWidget(new QLabel(tr("<b>选择安装位置</b>"), p));
    l->addWidget(new QLabel(tr("安装目录:"), p));

    auto* r = new QHBoxLayout();
    m_pathEdit = new QLineEdit(def, p);
    m_pathEdit->setStyleSheet(QStringLiteral("QLineEdit{padding:6px;}"));
    r->addWidget(m_pathEdit, 1);

    auto* b = new QPushButton(tr("浏览(&B)..."), p);
    connect(b, &QPushButton::clicked, this, [this]() {
        QString d = QFileDialog::getExistingDirectory(this, tr("选择安装目录"), m_pathEdit->text());
        if (!d.isEmpty()) m_pathEdit->setText(QDir::toNativeSeparators(d));
    });
    r->addWidget(b); l->addLayout(r); l->addStretch(); return p;
}

QWidget* InstallWizard::buildProgressPage()
{
    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p);
    l->setContentsMargins(0, 0, 0, 0); l->setSpacing(10);
    l->addWidget(new QLabel(tr("<b>正在安装...</b>"), p));

    m_statusLabel = new QLabel(tr("正在准备..."), p);
    m_statusLabel->setWordWrap(true); m_statusLabel->setStyleSheet(QStringLiteral("color:#aaa;"));
    l->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(p); m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0); m_progressBar->setMinimumHeight(18);
    l->addWidget(m_progressBar);

    // Detail toggle
    auto* detailBtn = new QPushButton(tr("显示详情 ▸"), p);
    detailBtn->setFlat(true);
    m_detailLog = new QTextEdit(p);
    m_detailLog->setReadOnly(true); m_detailLog->setVisible(false);
    m_detailLog->setMaximumHeight(120);
    connect(detailBtn, &QPushButton::clicked, this, [detailBtn, this]() {
        bool show = !m_detailLog->isVisible();
        m_detailLog->setVisible(show);
        detailBtn->setText(show ? tr("隐藏详情 ▾") : tr("显示详情 ▸"));
    });
    l->addWidget(detailBtn);
    l->addWidget(m_detailLog);

    l->addStretch(); return p;
}

QWidget* InstallWizard::buildFinishPage()
{
    const auto inst = m_package.value(QStringLiteral("installer")).toObject();
    const QString name = inst.value(QStringLiteral("name")).toString();
    const auto fa = m_package.value(QStringLiteral("finish_actions")).toObject();
    const bool runDef = fa.value(QStringLiteral("run_program")).toBool(true);
    const bool restart = fa.value(QStringLiteral("restart_required")).toBool(false);
    QString ft = inst.value(QStringLiteral("finish_title")).toString();

    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p);
    l->setContentsMargins(0, 0, 0, 0); l->setSpacing(12);
    l->addWidget(new QLabel(QStringLiteral("<h3>%1</h3>").arg(
        ft.isEmpty() ? tr("安装完成") : ft), p));

    QString fm = m_package.value(QStringLiteral("finish_message")).toString();
    if (fm.isEmpty()) fm = tr("%1 已成功安装。").arg(name);
    fm.replace(QStringLiteral("{name}"), name);
    auto* msg = new QLabel(fm, p); msg->setWordWrap(true);
    l->addWidget(msg);

    if (restart) {
        auto* rh = new QLabel(tr("<b>注意：</b>建议重新启动计算机以完成安装。"), p);
        rh->setWordWrap(true); l->addWidget(rh);
    }

    m_runCheckBox = new QCheckBox(tr("运行 %1(&R)").arg(name), p);
    m_runCheckBox->setChecked(runDef); l->addWidget(m_runCheckBox);
    l->addStretch(); return p;
}

QWidget* InstallWizard::buildUninstallConfirmPage()
{
    const QString name = m_package.value(QStringLiteral("installer")).toObject()
                             .value(QStringLiteral("name")).toString();
    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p);
    l->setContentsMargins(0, 0, 0, 0); l->setSpacing(12);
    l->addWidget(new QLabel(tr("<h3>卸载 %1</h3>").arg(name), p));
    l->addWidget(new QLabel(tr("<p>确定要卸载 <b>%1</b> 吗？</p>"
                               "<p>安装目录: %2</p>"
                               "<p>此操作将删除所有程序文件和相关设置。</p>")
        .arg(name, QDir::toNativeSeparators(m_uninstallDir)), p));
    l->addStretch(); return p;
}

QWidget* InstallWizard::buildUninstallProgressPage()
{
    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p);
    l->setContentsMargins(0, 0, 0, 0); l->setSpacing(12);
    l->addWidget(new QLabel(tr("<b>正在卸载...</b>"), p));
    m_statusLabel = new QLabel(tr("正在准备..."), p); m_statusLabel->setWordWrap(true);
    l->addWidget(m_statusLabel);
    m_progressBar = new QProgressBar(p); m_progressBar->setRange(0,100);
    m_progressBar->setValue(0); m_progressBar->setMinimumHeight(20);
    l->addWidget(m_progressBar); l->addStretch(); return p;
}

QWidget* InstallWizard::buildUninstallFinishPage()
{
    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p);
    l->setContentsMargins(0, 0, 0, 0); l->setSpacing(12);
    l->addWidget(new QLabel(tr("<h3>卸载完成</h3>"), p));
    l->addWidget(new QLabel(tr("程序已从您的计算机中移除。"), p));
    l->addStretch(); return p;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Navigation
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::navigateTo(int step)
{
    if (step < 0 || step >= m_stack->count()) return;
    m_currentStep = step; m_stack->setCurrentIndex(step);
    refreshStepIndicator(); refreshButtons();
}

void InstallWizard::onBack() { if (m_currentStep > 0) navigateTo(m_currentStep - 1); }

void InstallWizard::onNext()
{
    if (m_currentStep >= m_stack->count() - 1) { accept(); return; }
    if (m_mode == Install && m_currentStep == 2) { startInstallation(m_pathEdit->text()); return; }
    if (m_mode == Uninstall && m_currentStep == 0) {
        auto* e = new InstallerEngine; auto* t = new QThread(this);
        e->loadPackage(); e->moveToThread(t);
        connect(e, &InstallerEngine::progressChanged, this, &InstallWizard::onProgressChanged);
        connect(e, &InstallerEngine::statusChanged,   this, &InstallWizard::onStatusChanged);
        connect(e, &InstallerEngine::finished,         this, &InstallWizard::onFinished);
        navigateTo(1);
        QString d = m_uninstallDir;
        connect(t, &QThread::started, e, [e,d]() { e->uninstall(d); });
        connect(e, &InstallerEngine::finished, t, [t,e]() { e->deleteLater(); t->quit(); });
        connect(t, &QThread::finished, t, &QObject::deleteLater);
        t->start(); return;
    }
    navigateTo(m_currentStep + 1);
}

void InstallWizard::refreshButtons()
{
    int last = m_stack->count() - 1;
    bool first = (m_currentStep == 0), lastP = (m_currentStep >= last);
    bool prog = (m_mode == Install && m_currentStep == 3) || (m_mode == Uninstall && m_currentStep == 1);

    m_backBtn->setVisible(!first && !prog);
    m_nextBtn->setEnabled(true);

    if (prog) {
        m_nextBtn->setVisible(false);
    } else if (lastP) {
        m_nextBtn->setText(tr("完成(&F)")); m_nextBtn->setVisible(true);
    } else if ((m_mode == Install && m_currentStep == 2) || (m_mode == Uninstall && m_currentStep == 0)) {
        m_nextBtn->setText((m_mode == Install) ? tr("安装(&I)") : tr("卸载(&U)"));
        m_nextBtn->setVisible(true);
    } else {
        m_nextBtn->setText(tr("下一步(&N) >")); m_nextBtn->setVisible(true);
    }

    if (m_mode == Install && m_currentStep == 1 && m_licenseBox)
        m_nextBtn->setEnabled(m_licenseBox->isChecked());

    m_cancelBtn->setVisible(!lastP && !prog);
}

void InstallWizard::refreshStepIndicator()
{
    QLayoutItem* c;
    while ((c = m_stepLayout->takeAt(0)) != nullptr) { delete c->widget(); delete c; }

    const QStringList& steps = (m_mode == Install) ? m_installSteps : m_uninstallSteps;
    for (int i = 0; i < steps.size(); i++) {
        auto* row = new QHBoxLayout; row->setSpacing(10);

        auto* icon = new QLabel(this); icon->setFixedWidth(20); icon->setAlignment(Qt::AlignCenter);
        QString color;
        if (i == m_currentStep)      { icon->setText(QStringLiteral("\u25CF")); color = QStringLiteral("#2196f3"); }
        else                         { icon->setText(QStringLiteral("\u25CB")); color = QStringLiteral("#666"); }
        icon->setStyleSheet(QStringLiteral("color:%1;font-size:14px;font-weight:bold;").arg(color));
        row->addWidget(icon);

        auto* label = new QLabel(steps[i], this);
        if (i == m_currentStep)
            label->setStyleSheet(QStringLiteral("color:#fff;font-weight:bold;"));
        else
            label->setStyleSheet(QStringLiteral("color:#888;"));
        row->addWidget(label, 1); row->addStretch();
        m_stepLayout->addLayout(row);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Shared
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
            } else { navigateTo(4); }
            QMessageBox::information(this, tr("完成"),
                m_package.value(QStringLiteral("finish_message")).toString());
            accept();
        } else {
            QMessageBox::critical(this, tr("错误"), tr("需要管理员权限但无法提升。"));
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
    connect(engine, &InstallerEngine::finished, thread, [thread,engine](){ engine->deleteLater(); thread->quit(); });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void InstallWizard::onProgressChanged(int percent)
{
    if (m_style == OneClick) { if (m_ocProgressBar) m_ocProgressBar->setValue(percent); }
    else { if (m_progressBar) m_progressBar->setValue(percent); }
}

void InstallWizard::onStatusChanged(const QString& text)
{
    if (m_style == OneClick) { if (m_ocStatusLabel) m_ocStatusLabel->setText(text); }
    else {
        if (m_statusLabel) m_statusLabel->setText(text);
        if (m_detailLog && m_detailLog->isVisible())
            m_detailLog->append(text);
    }
}

void InstallWizard::onFinished(bool success, const QString& error)
{
    if (!success) { QMessageBox::critical(this, tr("安装失败"), error); return; }
    if (m_style == OneClick) {
        QMessageBox::information(this, tr("完成"),
            m_package.value(QStringLiteral("finish_message")).toString());
        accept();
    } else {
        if (m_progressBar) m_progressBar->setValue(100);
        if (m_statusLabel) m_statusLabel->setText(tr("安装完成！"));
        m_backBtn->setVisible(false); m_cancelBtn->setVisible(false);
        m_nextBtn->setText(tr("完成(&F)")); m_nextBtn->setVisible(true); m_nextBtn->setEnabled(true);
    }
}

void InstallWizard::done(int result)
{
    if (result == QDialog::Accepted && m_mode == Install && m_runCheckBox && m_runCheckBox->isChecked()) {
        QString dir = (m_style == OneClick) ? m_ocPathEdit->text() : m_pathEdit->text();
        QString exe = QDir(dir).absoluteFilePath(
            m_package.value(QStringLiteral("installer")).toObject()
                .value(QStringLiteral("main_executable")).toString());
        if (QFile::exists(exe)) QProcess::startDetached(exe, {}, dir);
    }
    QDialog::done(result);
}
