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
//  Constructor
// ═══════════════════════════════════════════════════════════════════════════════

InstallWizard::InstallWizard(Mode mode, const QJsonObject& pkg,
                             const QString& uninstDir, QWidget* parent)
    : QDialog(parent), m_mode(mode), m_pkg(pkg), m_uninstDir(uninstDir)
{
    const auto& inst = m_pkg[QStringLiteral("installer")].toObject();
    m_steps = (m_mode==Install) ? QStringList{tr("欢迎"),tr("许可协议"),tr("安装位置"),tr("安装进度"),tr("完成")}
                                : QStringList{tr("确认卸载"),tr("卸载进度"),tr("完成")};

    QString ui = inst[QStringLiteral("ui_style")].toString(QStringLiteral("wizard"));
    m_style = (ui==QStringLiteral("oneclick") && m_mode==Install) ? OneClick : WizardStyle;

    const QString name = inst[QStringLiteral("name")].toString();
    setWindowTitle((m_mode==Install) ? tr("%1 安装程序").arg(name) : tr("%1 卸载程序").arg(name));

    if (m_style == OneClick) setupOneClick(); else setupWizard();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  One-Click
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::setupOneClick()
{
    const auto& inst = m_pkg[QStringLiteral("installer")].toObject();
    const QString name   = inst[QStringLiteral("name")].toString();
    const QString ver    = inst[QStringLiteral("version")].toString();
    const QString pub    = inst[QStringLiteral("publisher")].toString();
    const QString defPath= QDir::toNativeSeparators(
        QDir(InstallerEngine::resolvePath(inst[QStringLiteral("default_install_root")].toString()))
            .absoluteFilePath(inst[QStringLiteral("default_install_folder")].toString()));

    setMinimumSize(640, 400); resize(700, 440);

    auto* h = new QHBoxLayout(this); h->setContentsMargins(24,20,24,16); h->setSpacing(24);

    // ── Left ──────────────────────────────────────────────────────────────
    auto* left = new QVBoxLayout; left->setSpacing(10);

    auto* title = new QLabel(tr("<h3>%1 %2</h3>").arg(name,ver), this);
    auto* sub   = new QLabel(pub, this); sub->setStyleSheet(QStringLiteral("color:#888;"));
    left->addWidget(title); left->addWidget(sub);

    auto* s1 = new QFrame(this); s1->setFrameShape(QFrame::HLine); left->addWidget(s1);

    auto* pathRow = new QHBoxLayout;
    m_ocPath = new QLineEdit(defPath, this);
    pathRow->addWidget(m_ocPath, 1);
    auto* browse = new QPushButton(tr("浏览..."), this);
    connect(browse, &QPushButton::clicked, this, [this](){
        QString d = QFileDialog::getExistingDirectory(this, tr("选择安装目录"), m_ocPath->text());
        if (!d.isEmpty()) m_ocPath->setText(QDir::toNativeSeparators(d));
    });
    pathRow->addWidget(browse);
    left->addWidget(new QLabel(tr("安装目录:"), this));
    left->addLayout(pathRow);

    auto* optGrp = new QGroupBox(tr("安装选项"), this);
    auto* optLay = new QVBoxLayout(optGrp);
    auto* cb1 = new QCheckBox(tr("创建桌面快捷方式"), this); cb1->setChecked(true);
    auto* cb2 = new QCheckBox(tr("创建开始菜单快捷方式"), this); cb2->setChecked(true);
    auto* cb3 = new QCheckBox(tr("开机自启"), this);
    optLay->addWidget(cb1); optLay->addWidget(cb2); optLay->addWidget(cb3);
    left->addWidget(optGrp);

    auto* licRow = new QHBoxLayout;
    m_ocLicense = new QCheckBox(tr("我已阅读并同意"), this);
    licRow->addWidget(m_ocLicense);
    auto* licBtn = new QPushButton(tr("许可协议"), this);
    licBtn->setFlat(true); licBtn->setStyleSheet(QStringLiteral("color:#06c;text-decoration:underline;border:none;"));
    connect(licBtn, &QPushButton::clicked, this, &InstallWizard::onShowLicense);
    licRow->addWidget(licBtn); licRow->addStretch();
    left->addLayout(licRow);
    left->addStretch();

    // ── Right ─────────────────────────────────────────────────────────────
    auto* right = new QVBoxLayout; right->setSpacing(12); right->addStretch();

    m_ocBtn = new QPushButton(tr("立即安装"), this);
    m_ocBtn->setMinimumSize(160, 120);
    m_ocBtn->setStyleSheet(QStringLiteral("QPushButton{font-size:16pt;font-weight:bold;padding:20px 40px;border-radius:8px}"));
    m_ocBtn->setEnabled(false);
    connect(m_ocBtn, &QPushButton::clicked, this, &InstallWizard::onOneClickGo);
    connect(m_ocLicense, &QCheckBox::toggled, m_ocBtn, &QPushButton::setEnabled);

    auto* ctr = new QHBoxLayout; ctr->addStretch(); ctr->addWidget(m_ocBtn); ctr->addStretch();
    right->addLayout(ctr);

    m_ocBar = new QProgressBar(this); m_ocBar->setRange(0,100); m_ocBar->setValue(0);
    m_ocBar->setVisible(false); m_ocBar->setMinimumWidth(200);
    right->addWidget(m_ocBar);
    m_ocStatus = new QLabel(this); m_ocStatus->setWordWrap(true); m_ocStatus->setVisible(false);
    right->addWidget(m_ocStatus);
    right->addStretch();

    h->addLayout(left, 3); h->addLayout(right, 2);
}

void InstallWizard::onShowLicense()
{
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("许可协议")); dlg->resize(500,400);
    auto* lay = new QVBoxLayout(dlg);
    auto* edit = new QTextEdit(dlg); edit->setReadOnly(true);
    QString lf = m_pkg[QStringLiteral("installer")].toObject()[QStringLiteral("license_file")].toString();
    if (!lf.isEmpty()) {
        QFile f(QStringLiteral(":/package/files/%1").arg(lf));
        if (f.open(QIODevice::ReadOnly|QIODevice::Text)) edit->setPlainText(QString::fromUtf8(f.readAll()));
    } else edit->setPlainText(tr("(未提供许可协议)"));
    lay->addWidget(edit);
    auto* close = new QPushButton(tr("关闭"), dlg); connect(close,&QPushButton::clicked,dlg,&QDialog::accept);
    lay->addWidget(close); dlg->exec(); dlg->deleteLater();
}

void InstallWizard::onOneClickGo()
{
    m_ocPath->setEnabled(false); m_ocLicense->setEnabled(false); m_ocBtn->setVisible(false);
    m_ocBar->setVisible(true); m_ocStatus->setVisible(true);
    startInstall(m_ocPath->text());
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Wizard
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::setupWizard()
{
    setMinimumSize(680, 480); resize(740, 520);

    auto* h = new QHBoxLayout(this); h->setContentsMargins(0,0,0,0); h->setSpacing(0);

    // ── Left panel ────────────────────────────────────────────────────────
    auto* leftPanel = new QFrame(this);
    leftPanel->setFixedWidth(170); leftPanel->setFrameShape(QFrame::NoFrame);
    { auto p = leftPanel->palette(); p.setColor(QPalette::Window, QColor(42,42,46));
      leftPanel->setPalette(p); leftPanel->setAutoFillBackground(true); }
    auto* leftLay = new QVBoxLayout(leftPanel);
    leftLay->setContentsMargins(16,24,16,20); leftLay->setSpacing(0);

    auto* head = new QLabel(tr("安装步骤"), leftPanel);
    head->setStyleSheet(QStringLiteral("color:#999;font-size:11px;margin-bottom:4px;"));
    leftLay->addWidget(head);
    auto* hl = new QFrame(leftPanel); hl->setFrameShape(QFrame::HLine);
    hl->setStyleSheet(QStringLiteral("border-color:#444;"));
    leftLay->addWidget(hl); leftLay->addSpacing(10);

    m_stepLayout = new QVBoxLayout; m_stepLayout->setSpacing(2);
    leftLay->addLayout(m_stepLayout);
    leftLay->addStretch();
    h->addWidget(leftPanel);

    // ── Right ─────────────────────────────────────────────────────────────
    auto* right = new QFrame(this); right->setFrameShape(QFrame::NoFrame);
    auto* rl = new QVBoxLayout(right);
    rl->setContentsMargins(28,24,28,12); rl->setSpacing(0);

    m_stack = new QStackedWidget(this);
    rl->addWidget(m_stack, 1);

    auto* bl = new QFrame(this); bl->setFrameShape(QFrame::HLine);
    rl->addWidget(bl); rl->addSpacing(10);

    auto* btns = new QHBoxLayout; btns->setSpacing(8); btns->addStretch();
    m_btnBack   = new QPushButton(tr("< 上一步(&B)"), this);
    m_btnNext   = new QPushButton(tr("下一步(&N) >"), this);
    m_btnCancel = new QPushButton(tr("取消"), this);
    m_btnBack->setMinimumWidth(110); m_btnNext->setMinimumWidth(110);
    m_btnCancel->setMinimumWidth(80); m_btnNext->setDefault(true);
    btns->addWidget(m_btnBack); btns->addWidget(m_btnNext); btns->addWidget(m_btnCancel);
    rl->addLayout(btns);
    h->addWidget(right, 1);

    connect(m_btnBack,   &QPushButton::clicked, this, &InstallWizard::onWizardBack);
    connect(m_btnNext,   &QPushButton::clicked, this, &InstallWizard::onWizardNext);
    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    // Pages
    if (m_mode == Install) {
        m_stack->addWidget(pageWelcome());
        m_stack->addWidget(pageLicense());
        m_stack->addWidget(pagePath());
        m_stack->addWidget(pageProgress());
        m_stack->addWidget(pageFinish());
    } else {
        m_stack->addWidget(pageUninstConfirm());
        m_stack->addWidget(pageUninstProgress());
        m_stack->addWidget(pageUninstFinish());
    }

    refreshSteps();
    m_stack->setCurrentIndex(0);
    refreshWizardNav();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Wizard pages
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* InstallWizard::pageWelcome()
{
    const auto& inst = m_pkg[QStringLiteral("installer")].toObject();
    const QString name = inst[QStringLiteral("name")].toString();
    const QString ver  = inst[QStringLiteral("version")].toString();
    const QString pub  = inst[QStringLiteral("publisher")].toString();
    const QString copy = inst[QStringLiteral("copyright")].toString();
    QString wt = inst[QStringLiteral("welcome_title")].toString();
    QString wx = inst[QStringLiteral("welcome_text")].toString();

    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p); l->setSpacing(12);
    l->addWidget(new QLabel(QStringLiteral("<h3>%1</h3>").arg(
        wt.isEmpty() ? tr("欢迎使用 %1 安装向导").arg(name) : wt), p));
    l->addWidget(new QLabel(wx.isEmpty()
        ? tr("<p>本向导将引导您完成 <b>%1</b> 的安装。</p>").arg(name) : wx, p));
    auto* info = new QLabel(tr("版本: %1 &nbsp;|&nbsp; 发布者: %2").arg(ver, pub), p);
    info->setStyleSheet(QStringLiteral("color:#888;"));
    l->addWidget(info);
    if (!copy.isEmpty()) { auto* c = new QLabel(copy,p); c->setStyleSheet(QStringLiteral("color:#777;")); l->addWidget(c); }
    l->addStretch(); return p;
}

QWidget* InstallWizard::pageLicense()
{
    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p); l->setSpacing(8);
    l->addWidget(new QLabel(tr("<b>请阅读以下许可协议：</b>"), p));
    auto* e = new QTextEdit(p); e->setReadOnly(true);
    e->setStyleSheet(QStringLiteral("QTextEdit{border:1px solid #555;border-radius:4px;padding:8px;}"));
    QString lf = m_pkg[QStringLiteral("installer")].toObject()[QStringLiteral("license_file")].toString();
    if (!lf.isEmpty()) { QFile f(QStringLiteral(":/package/files/%1").arg(lf));
        if (f.open(QIODevice::ReadOnly|QIODevice::Text)) e->setPlainText(QString::fromUtf8(f.readAll())); }
    l->addWidget(e, 1);
    m_chkLicense = new QCheckBox(tr("我接受许可协议的条款(&A)"), p);
    connect(m_chkLicense, &QCheckBox::toggled, this, [this](){ refreshWizardNav(); });
    l->addWidget(m_chkLicense);
    return p;
}

QWidget* InstallWizard::pagePath()
{
    const auto& inst = m_pkg[QStringLiteral("installer")].toObject();
    QString def = QDir::toNativeSeparators(
        QDir(InstallerEngine::resolvePath(inst[QStringLiteral("default_install_root")].toString()))
            .absoluteFilePath(inst[QStringLiteral("default_install_folder")].toString()));

    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p); l->setSpacing(10);
    l->addWidget(new QLabel(tr("<b>选择安装位置</b>"), p));
    l->addWidget(new QLabel(tr("安装目录:"), p));
    auto* row = new QHBoxLayout;
    m_editPath = new QLineEdit(def, p);
    row->addWidget(m_editPath, 1);
    auto* btn = new QPushButton(tr("浏览(&B)..."), p);
    connect(btn, &QPushButton::clicked, this, [this](){
        QString d = QFileDialog::getExistingDirectory(this, tr("选择安装目录"), m_editPath->text());
        if (!d.isEmpty()) m_editPath->setText(QDir::toNativeSeparators(d));
    });
    row->addWidget(btn);
    l->addLayout(row); l->addStretch(); return p;
}

QWidget* InstallWizard::pageProgress()
{
    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p); l->setSpacing(10);
    l->addWidget(new QLabel(tr("<b>正在安装...</b>"), p));
    m_lblStatus = new QLabel(tr("正在准备..."), p); m_lblStatus->setStyleSheet(QStringLiteral("color:#aaa;"));
    l->addWidget(m_lblStatus);
    m_bar = new QProgressBar(p); m_bar->setRange(0,100); m_bar->setValue(0); m_bar->setMinimumHeight(18);
    l->addWidget(m_bar);
    auto* detailBtn = new QPushButton(tr("显示详情 ▸"),p); detailBtn->setFlat(true);
    m_log = new QTextEdit(p); m_log->setReadOnly(true); m_log->setVisible(false); m_log->setMaximumHeight(120);
    l->addWidget(detailBtn); l->addWidget(m_log);
    connect(detailBtn, &QPushButton::clicked, this, [detailBtn,this](){
        bool vis = !m_log->isVisible(); m_log->setVisible(vis);
        detailBtn->setText(vis ? tr("隐藏详情 ▾") : tr("显示详情 ▸"));
    });
    l->addStretch(); return p;
}

QWidget* InstallWizard::pageFinish()
{
    const auto& inst = m_pkg[QStringLiteral("installer")].toObject();
    const QString name = inst[QStringLiteral("name")].toString();
    const auto& fa = m_pkg[QStringLiteral("finish_actions")].toObject();
    bool runDef = fa[QStringLiteral("run_program")].toBool(true);
    bool restart = fa[QStringLiteral("restart_required")].toBool(false);
    QString ft = inst[QStringLiteral("finish_title")].toString();

    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p); l->setSpacing(12);
    l->addWidget(new QLabel(QStringLiteral("<h3>%1</h3>").arg(ft.isEmpty()?tr("安装完成"):ft), p));
    QString fm = m_pkg[QStringLiteral("finish_message")].toString();
    if (fm.isEmpty()) fm = tr("%1 已成功安装。").arg(name);
    fm.replace(QStringLiteral("{name}"), name);
    l->addWidget(new QLabel(fm, p));
    if (restart) l->addWidget(new QLabel(tr("<b>注意：</b>建议重新启动计算机以完成安装。"), p));
    l->addSpacing(8);
    m_chkRun = new QCheckBox(tr("运行 %1(&R)").arg(name), p); m_chkRun->setChecked(runDef);
    l->addWidget(m_chkRun);
    l->addStretch(); return p;
}

QWidget* InstallWizard::pageUninstConfirm()
{
    const QString name = m_pkg[QStringLiteral("installer")].toObject()[QStringLiteral("name")].toString();
    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p); l->setSpacing(12);
    l->addWidget(new QLabel(tr("<h3>卸载 %1</h3>").arg(name), p));
    l->addWidget(new QLabel(tr("<p>确定要卸载 <b>%1</b> 吗？</p><p>安装目录: %2</p>"
        "<p>此操作将删除所有程序文件和设置。</p>").arg(name, QDir::toNativeSeparators(m_uninstDir)), p));
    l->addStretch(); return p;
}

QWidget* InstallWizard::pageUninstProgress()
{
    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p); l->setSpacing(10);
    l->addWidget(new QLabel(tr("<b>正在卸载...</b>"), p));
    m_lblStatus = new QLabel(tr("正在准备..."), p); m_lblStatus->setStyleSheet(QStringLiteral("color:#aaa;"));
    l->addWidget(m_lblStatus);
    m_bar = new QProgressBar(p); m_bar->setRange(0,100); m_bar->setValue(0); m_bar->setMinimumHeight(18);
    l->addWidget(m_bar);
    l->addStretch(); return p;
}

QWidget* InstallWizard::pageUninstFinish()
{
    auto* p = new QWidget(this); auto* l = new QVBoxLayout(p); l->setSpacing(12);
    l->addWidget(new QLabel(tr("<h3>卸载完成</h3>"), p));
    l->addWidget(new QLabel(tr("程序已从您的计算机中移除。"), p));
    l->addStretch(); return p;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Navigation
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::refreshSteps()
{
    QLayoutItem* ci;
    while ((ci = m_stepLayout->takeAt(0)) != nullptr) { delete ci->widget(); delete ci; }

    for (int i = 0; i < m_steps.size(); i++) {
        auto* row = new QHBoxLayout; row->setSpacing(10);
        auto* dot = new QLabel(this); dot->setFixedWidth(18); dot->setAlignment(Qt::AlignCenter);
        bool cur = (i == m_step);
        dot->setText(cur ? QStringLiteral("\u25CF") : QStringLiteral("\u25CB"));
        dot->setStyleSheet(QStringLiteral("color:%1;font-size:13px;font-weight:bold;")
            .arg(cur ? QStringLiteral("#2196f3") : QStringLiteral("#555")));
        row->addWidget(dot);
        auto* lb = new QLabel(m_steps[i], this);
        lb->setStyleSheet(QStringLiteral("color:%1;%2").arg(cur ? QStringLiteral("#fff") : QStringLiteral("#888"),
            cur ? QStringLiteral("font-weight:bold;") : QString()));
        row->addWidget(lb, 1); row->addStretch();
        m_stepLayout->addLayout(row);
    }
}

void InstallWizard::refreshWizardNav()
{
    int last = m_stack->count() - 1;
    bool first = (m_step == 0), lastP = (m_step >= last);
    bool progress = (m_mode==Install && m_step==3) || (m_mode==Uninstall && m_step==1);

    m_btnBack->setVisible(!first && !progress);
    m_btnCancel->setVisible(!lastP && !progress);
    m_btnNext->setEnabled(true);
    m_btnNext->setVisible(true);

    if (progress) {
        m_btnNext->setVisible(false);
    } else if (lastP) {
        m_btnNext->setText(tr("完成(&F)"));
    } else if ((m_mode==Install && m_step==2) || (m_mode==Uninstall && m_step==0)) {
        m_btnNext->setText((m_mode==Install) ? tr("安装(&I)") : tr("卸载(&U)"));
    } else {
        m_btnNext->setText(tr("下一步(&N) >"));
    }

    // License acceptance
    if (m_mode==Install && m_step==1 && m_chkLicense)
        m_btnNext->setEnabled(m_chkLicense->isChecked());
}

void InstallWizard::onWizardBack()
{
    if (m_step > 0) { m_step--; m_stack->setCurrentIndex(m_step); refreshSteps(); refreshWizardNav(); }
}

void InstallWizard::onWizardNext()
{
    int last = m_stack->count() - 1;
    if (m_step >= last) { accept(); return; }

    // Install path → start install
    if (m_mode==Install && m_step==2) {
        m_step = 3; m_stack->setCurrentIndex(m_step); refreshSteps(); refreshWizardNav();
        startInstall(m_editPath->text());
        return;
    }
    // Uninstall confirm → start uninstall
    if (m_mode==Uninstall && m_step==0) {
        m_step = 1; m_stack->setCurrentIndex(m_step); refreshSteps(); refreshWizardNav();
        auto* eng = new InstallerEngine; auto* thr = new QThread(this);
        eng->loadPackage(); eng->moveToThread(thr);
        connect(eng,&InstallerEngine::progressChanged,this,&InstallWizard::onProgress);
        connect(eng,&InstallerEngine::statusChanged,this,&InstallWizard::onStatus);
        connect(eng,&InstallerEngine::finished,this,&InstallWizard::onEngineFinished);
        QString d = m_uninstDir;
        connect(thr,&QThread::started,eng,[eng,d](){ eng->uninstall(d); });
        connect(eng,&InstallerEngine::finished,thr,[thr,eng](){ eng->deleteLater(); thr->quit(); });
        connect(thr,&QThread::finished,thr,&QObject::deleteLater);
        thr->start(); return;
    }

    m_step++; m_stack->setCurrentIndex(m_step); refreshSteps(); refreshWizardNav();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Install
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::startInstall(const QString& dir)
{
    const auto& inst = m_pkg[QStringLiteral("installer")].toObject();
    bool force = inst[QStringLiteral("requires_admin")].toBool(false);
    if (force || !InstallerEngine::canWriteToDir(dir)) {
        if (InstallerEngine::elevateSelf(dir, m_pkg)) {
            QMessageBox::information(this, tr("完成"),
                m_pkg[QStringLiteral("finish_message")].toString());
            accept(); return;
        }
        QMessageBox::critical(this, tr("错误"), tr("需要管理员权限但无法提升。"));
        return;
    }

    auto* eng = new InstallerEngine; auto* thr = new QThread(this);
    eng->loadPackage(); eng->moveToThread(thr);
    connect(eng,&InstallerEngine::progressChanged,this,&InstallWizard::onProgress);
    connect(eng,&InstallerEngine::statusChanged,this,&InstallWizard::onStatus);
    connect(eng,&InstallerEngine::finished,this,&InstallWizard::onEngineFinished);
    connect(thr,&QThread::started,eng,[eng,dir](){ eng->install(dir); });
    connect(eng,&InstallerEngine::finished,thr,[thr,eng](){ eng->deleteLater(); thr->quit(); });
    connect(thr,&QThread::finished,thr,&QObject::deleteLater);
    thr->start();
}

void InstallWizard::onProgress(int pct)
{
    if (m_style==OneClick && m_ocBar) m_ocBar->setValue(pct);
    if (m_bar) m_bar->setValue(pct);
}

void InstallWizard::onStatus(const QString& msg)
{
    if (m_style==OneClick && m_ocStatus) m_ocStatus->setText(msg);
    if (m_lblStatus) m_lblStatus->setText(msg);
    if (m_log && m_log->isVisible()) m_log->append(msg);
}

void InstallWizard::onEngineFinished(bool ok, const QString& err)
{
    if (!ok) { QMessageBox::critical(this, tr("错误"), err); return; }
    if (m_style == OneClick) {
        QMessageBox::information(this, tr("完成"), m_pkg[QStringLiteral("finish_message")].toString());
        accept();
    } else {
        m_lblStatus->setText(tr("完成！"));
        m_bar->setValue(100);
        m_btnBack->setVisible(false); m_btnCancel->setVisible(false);
        m_btnNext->setText(tr("完成(&F)")); m_btnNext->setVisible(true); m_btnNext->setEnabled(true);
    }
}

void InstallWizard::done(int result)
{
    if (result == QDialog::Accepted && m_mode == Install && m_chkRun && m_chkRun->isChecked()) {
        QString dir = (m_style == OneClick) ? m_ocPath->text() : m_editPath->text();
        QString exe = QDir(dir).absoluteFilePath(
            m_pkg[QStringLiteral("installer")].toObject()[QStringLiteral("main_executable")].toString());
        if (QFile::exists(exe)) QProcess::startDetached(exe, {}, dir);
    }
    QDialog::done(result);
}
