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

InstallWizard::InstallWizard(Mode mode,
                             const QJsonObject& package,
                             const QString& uninstallDir,
                             QWidget* parent)
    : QDialog(parent)
    , m_mode(mode)
    , m_package(package)
    , m_uninstallDir(uninstallDir)
{
    m_installSteps   = { tr("欢迎"), tr("许可协议"), tr("安装位置"),
                         tr("安装进度"), tr("完成") };
    m_uninstallSteps = { tr("确认卸载"), tr("卸载进度"), tr("完成") };

    setupUi();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  UI Setup
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::setupUi()
{
    const QString appName =
        m_package.value(QStringLiteral("installer")).toObject()
            .value(QStringLiteral("name")).toString();

    if (m_mode == Install)
        setWindowTitle(tr("%1 安装程序").arg(appName));
    else
        setWindowTitle(tr("%1 卸载程序").arg(appName));

    setMinimumSize(640, 440);
    resize(700, 480);

    // ── Main layout: [left panel] | [right panel] ────────────────────────
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // ── Left step panel ──────────────────────────────────────────────────
    auto* leftPanel = new QFrame(this);
    leftPanel->setFixedWidth(180);
    leftPanel->setFrameShape(QFrame::NoFrame);
    {
        auto pal = leftPanel->palette();
        pal.setColor(QPalette::Window, pal.color(QPalette::Window).darker(108));
        leftPanel->setPalette(pal);
        leftPanel->setAutoFillBackground(true);
    }
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(20, 30, 20, 20);
    leftLayout->setSpacing(4);

    m_stepLayout = new QVBoxLayout();
    m_stepLayout->setSpacing(4);
    leftLayout->addLayout(m_stepLayout);
    leftLayout->addStretch();

    mainLayout->addWidget(leftPanel);

    // ── Right content area ───────────────────────────────────────────────
    auto* rightPanel = new QFrame(this);
    rightPanel->setFrameShape(QFrame::NoFrame);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(24, 20, 24, 16);

    m_stack = new QStackedWidget(this);
    rightLayout->addWidget(m_stack, 1);

    // ── Bottom button bar ────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    btnRow->addStretch();

    m_backBtn = new QPushButton(tr("< 上一步(&B)"), this);
    m_nextBtn = new QPushButton(tr("下一步(&N) >"), this);
    m_cancelBtn = new QPushButton(tr("取消"), this);

    m_backBtn->setMinimumWidth(100);
    m_nextBtn->setMinimumWidth(100);
    m_cancelBtn->setMinimumWidth(80);

    btnRow->addWidget(m_backBtn);
    btnRow->addWidget(m_nextBtn);
    btnRow->addWidget(m_cancelBtn);
    rightLayout->addLayout(btnRow);

    mainLayout->addWidget(rightPanel, 1);

    // ── Connections ──────────────────────────────────────────────────────
    connect(m_backBtn,   &QPushButton::clicked, this, &InstallWizard::onBack);
    connect(m_nextBtn,   &QPushButton::clicked, this, &InstallWizard::onNext);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

    // ── Build pages ──────────────────────────────────────────────────────
    if (m_mode == Install) {
        m_stack->addWidget(buildWelcomePage());       // 0
        m_stack->addWidget(buildLicensePage());       // 1
        m_stack->addWidget(buildPathPage());          // 2
        m_stack->addWidget(buildProgressPage());      // 3
        m_stack->addWidget(buildFinishPage());        // 4
    } else {
        m_stack->addWidget(buildUninstallConfirmPage());   // 0
        m_stack->addWidget(buildUninstallProgressPage());  // 1
        m_stack->addWidget(buildUninstallFinishPage());    // 2
    }

    // ── Init ─────────────────────────────────────────────────────────────
    refreshStepIndicator();
    navigateTo(0);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Navigation
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::navigateTo(int step)
{
    const int count = m_stack->count();
    if (step < 0 || step >= count) return;

    m_currentStep = step;
    m_stack->setCurrentIndex(step);
    refreshStepIndicator();
    refreshButtons();
}

void InstallWizard::onBack()
{
    if (m_currentStep > 0)
        navigateTo(m_currentStep - 1);
}

void InstallWizard::onNext()
{
    const QStringList& steps = (m_mode == Install) ? m_installSteps : m_uninstallSteps;
    const int lastPage = m_stack->count() - 1;

    if (m_currentStep >= lastPage) {
        accept();
        return;
    }

    // ── Install: path page → start installation ──────────────────────────
    if (m_mode == Install && m_currentStep == 2) {
        startInstallation(m_pathEdit->text());
        return;
    }

    // ── Uninstall: confirm page → start uninstallation ───────────────────
    if (m_mode == Uninstall && m_currentStep == 0) {
        startUninstallation();
        return;
    }

    navigateTo(m_currentStep + 1);
}

void InstallWizard::refreshButtons()
{
    const int lastPage = m_stack->count() - 1;
    const bool isFirst    = (m_currentStep == 0);
    const bool isLast     = (m_currentStep >= lastPage);
    const bool isProgress = (m_mode == Install   && m_currentStep == 3)
                         || (m_mode == Uninstall && m_currentStep == 1);

    m_backBtn->setVisible(!isFirst && !isProgress);

    // Default: enable next
    m_nextBtn->setEnabled(true);

    if (isProgress) {
        m_nextBtn->setVisible(false);
    } else if (isLast) {
        m_nextBtn->setText(tr("完成(&F)"));
        m_nextBtn->setVisible(true);
    } else if ((m_mode == Install   && m_currentStep == 2)
            || (m_mode == Uninstall && m_currentStep == 0)) {
        m_nextBtn->setText((m_mode == Install)
            ? tr("安装(&I)") : tr("卸载(&U)"));
        m_nextBtn->setVisible(true);
    } else {
        m_nextBtn->setText(tr("下一步(&N) >"));
        m_nextBtn->setVisible(true);
    }

    // License page (Install step 1): enforce acceptance
    if (m_mode == Install && m_currentStep == 1 && m_licenseBox) {
        m_nextBtn->setEnabled(m_licenseBox->isChecked());
    }

    m_cancelBtn->setVisible(!isLast && !isProgress);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Step indicator (left panel)
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::refreshStepIndicator()
{
    // Clear old items
    QLayoutItem* child;
    while ((child = m_stepLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    const QStringList& steps = (m_mode == Install) ? m_installSteps : m_uninstallSteps;
    const int count = steps.size();

    for (int i = 0; i < count; ++i) {
        auto* row = new QHBoxLayout();
        row->setSpacing(10);

        // Icon / bullet
        auto* icon = new QLabel(this);
        icon->setFixedWidth(24);
        icon->setAlignment(Qt::AlignCenter);
        QFont iconFont = icon->font();
        iconFont.setPointSize(12);

        if (i < m_currentStep) {
            icon->setText(QStringLiteral("\u2713"));         // ✓
            iconFont.setBold(true);
        } else if (i == m_currentStep) {
            icon->setText(QStringLiteral("\u25CF"));         // ●
            iconFont.setBold(true);
        } else {
            icon->setText(QStringLiteral("\u25CB"));         // ○
        }
        icon->setFont(iconFont);
        row->addWidget(icon);

        // Text
        auto* label = new QLabel(steps.at(i), this);
        if (i == m_currentStep) {
            QFont f = label->font();
            f.setBold(true);
            label->setFont(f);
        }
        row->addWidget(label, 1);
        row->addStretch();

        m_stepLayout->addLayout(row);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Page builders — Install
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* InstallWizard::buildWelcomePage()
{
    const QJsonObject inst = m_package.value(QStringLiteral("installer")).toObject();
    const QString name     = inst.value(QStringLiteral("name")).toString();
    const QString ver      = inst.value(QStringLiteral("version")).toString();
    const QString pub      = inst.value(QStringLiteral("publisher")).toString();
    const QString copy     = inst.value(QStringLiteral("copyright")).toString();
    const QString wtitle   = inst.value(QStringLiteral("welcome_title")).toString();
    const QString wtext    = inst.value(QStringLiteral("welcome_text")).toString();

    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(0, 10, 0, 10);

    auto* title = new QLabel(wtitle.isEmpty()
        ? tr("<h2>欢迎使用 %1 安装向导</h2>").arg(name)
        : QStringLiteral("<h2>%1</h2>").arg(wtitle), page);
    title->setWordWrap(true);
    lay->addWidget(title);

    auto* info = new QLabel(wtext.isEmpty()
        ? tr("<p>本向导将引导您完成 <b>%1</b> 的安装。</p>").arg(name) : wtext, page);
    info->setWordWrap(true);
    lay->addWidget(info);

    auto* verLabel = new QLabel(
        tr("<p>版本: %1 &nbsp;|&nbsp; 发布者: %2</p>").arg(ver, pub), page);
    verLabel->setWordWrap(true);
    lay->addWidget(verLabel);

    if (!copy.isEmpty()) {
        auto* c = new QLabel(copy, page);
        c->setWordWrap(true);
        lay->addWidget(c);
    }

    lay->addStretch();
    return page;
}

QWidget* InstallWizard::buildLicensePage()
{
    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(0, 10, 0, 10);

    auto* title = new QLabel(tr("<b>请阅读以下许可协议：</b>"), page);
    lay->addWidget(title);

    auto* edit = new QTextEdit(page);
    edit->setReadOnly(true);

    const QString licenseFile = m_package.value(QStringLiteral("license_file")).toString();
    if (!licenseFile.isEmpty()) {
        QFile lf(QStringLiteral(":/package/files/%1").arg(licenseFile));
        if (lf.open(QIODevice::ReadOnly | QIODevice::Text))
            edit->setPlainText(QString::fromUtf8(lf.readAll()));
    }
    lay->addWidget(edit, 1);

    m_licenseBox = new QCheckBox(tr("我接受许可协议的条款(&A)"), page);
    lay->addWidget(m_licenseBox);

    connect(m_licenseBox, &QCheckBox::toggled, this, [this]() {
        refreshButtons();
    });

    return page;
}

QWidget* InstallWizard::buildPathPage()
{
    const auto inst = m_package.value(QStringLiteral("installer")).toObject();
    const QString defRoot = InstallerEngine::resolvePath(
        inst.value(QStringLiteral("default_install_root")).toString());
    const QString defFolder = inst.value(QStringLiteral("default_install_folder")).toString();

    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(0, 10, 0, 10);

    auto* title = new QLabel(tr("<b>选择安装位置</b>"), page);
    lay->addWidget(title);

    lay->addSpacing(8);

    auto* hint = new QLabel(tr("安装目录(&D):"), page);
    lay->addWidget(hint);

    auto* row = new QHBoxLayout();
    m_pathEdit = new QLineEdit(QDir::toNativeSeparators(
        QDir(defRoot).absoluteFilePath(defFolder)), page);
    row->addWidget(m_pathEdit, 1);

    auto* browseBtn = new QPushButton(tr("浏览(&B)..."), page);
    connect(browseBtn, &QPushButton::clicked, this, [this]() {
        const QString dir = QFileDialog::getExistingDirectory(
            this, tr("选择安装目录"), m_pathEdit->text());
        if (!dir.isEmpty())
            m_pathEdit->setText(QDir::toNativeSeparators(dir));
    });
    row->addWidget(browseBtn);
    lay->addLayout(row);

    lay->addStretch();
    return page;
}

QWidget* InstallWizard::buildProgressPage()
{
    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(0, 10, 0, 10);

    auto* title = new QLabel(tr("<b>正在安装...</b>"), page);
    lay->addWidget(title);

    m_statusLabel = new QLabel(tr("正在准备..."), page);
    m_statusLabel->setWordWrap(true);
    lay->addWidget(m_statusLabel);

    lay->addSpacing(12);

    m_progressBar = new QProgressBar(page);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    lay->addWidget(m_progressBar);

    lay->addStretch();
    return page;
}

QWidget* InstallWizard::buildFinishPage()
{
    const auto inst = m_package.value(QStringLiteral("installer")).toObject();
    const QString name     = inst.value(QStringLiteral("name")).toString();
    const auto fa = m_package.value(QStringLiteral("finish_actions")).toObject();
    const bool runDefault  = fa.value(QStringLiteral("run_program")).toBool(true);
    const bool needRestart = fa.value(QStringLiteral("restart_required")).toBool(false);
    const QString finTitle = inst.value(QStringLiteral("finish_title")).toString();

    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(0, 10, 0, 10);

    auto* title = new QLabel(
        QStringLiteral("<h2>%1</h2>").arg(finTitle.isEmpty() ? tr("安装完成") : finTitle), page);
    lay->addWidget(title);

    // Build finish message with variable substitution
    QString fm = m_package.value(QStringLiteral("finish_message")).toString();
    if (fm.isEmpty()) fm = tr("%1 已成功安装。").arg(name);
    fm.replace(QStringLiteral("{name}"), name);
    auto* msg = new QLabel(fm, page);
    msg->setWordWrap(true);
    lay->addWidget(msg);

    if (needRestart) {
        auto* rh = new QLabel(
            tr("<p><b>注意：</b>建议重新启动计算机以完成安装。</p>"), page);
        rh->setWordWrap(true);
        lay->addWidget(rh);
    }

    lay->addSpacing(16);

    m_runCheckBox = new QCheckBox(tr("运行 %1(&R)").arg(name), page);
    m_runCheckBox->setChecked(runDefault);
    lay->addWidget(m_runCheckBox);

    lay->addStretch();
    return page;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Page builders — Uninstall
// ═══════════════════════════════════════════════════════════════════════════════

QWidget* InstallWizard::buildUninstallConfirmPage()
{
    const QString name = m_package.value(QStringLiteral("installer")).toObject()
                             .value(QStringLiteral("name")).toString();

    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(0, 10, 0, 10);

    auto* title = new QLabel(tr("<h2>卸载 %1</h2>").arg(name), page);
    lay->addWidget(title);

    auto* info = new QLabel(
        tr("<p>您确定要卸载 <b>%1</b> 吗？</p>"
           "<p>安装目录: %2</p>"
           "<p>此操作将删除所有程序文件和相关注册表项。</p>")
            .arg(name, QDir::toNativeSeparators(m_uninstallDir)), page);
    info->setWordWrap(true);
    lay->addWidget(info);

    lay->addStretch();
    return page;
}

QWidget* InstallWizard::buildUninstallProgressPage()
{
    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(0, 10, 0, 10);

    auto* title = new QLabel(tr("<b>正在卸载...</b>"), page);
    lay->addWidget(title);

    m_statusLabel = new QLabel(tr("正在准备..."), page);
    m_statusLabel->setWordWrap(true);
    lay->addWidget(m_statusLabel);

    lay->addSpacing(12);

    m_progressBar = new QProgressBar(page);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    lay->addWidget(m_progressBar);

    lay->addStretch();
    return page;
}

QWidget* InstallWizard::buildUninstallFinishPage()
{
    auto* page = new QWidget(this);
    auto* lay  = new QVBoxLayout(page);
    lay->setContentsMargins(0, 10, 0, 10);

    auto* title = new QLabel(tr("<h2>卸载完成</h2>"), page);
    lay->addWidget(title);

    auto* msg = new QLabel(tr("程序已从您的计算机中移除。"), page);
    msg->setWordWrap(true);
    lay->addWidget(msg);

    lay->addStretch();
    return page;
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Install action
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::startInstallation(const QString& installDir)
{
    // Check admin rights (respect force-admin config)
    const bool forceAdmin = m_package.value(QStringLiteral("installer")).toObject()
                                .value(QStringLiteral("require_admin")).toBool(false);
    const bool needsAdmin = forceAdmin || !InstallerEngine::canWriteToDir(installDir);

    if (needsAdmin) {
        bool ok = InstallerEngine::elevateSelf(installDir, m_package);
        if (ok) {
            // Elevated install done → go straight to finish
            m_statusLabel->setText(tr("安装完成！"));
            m_progressBar->setValue(100);
            navigateTo(4);   // finish page
        } else {
            QMessageBox::critical(this, tr("错误"),
                tr("需要管理员权限但无法提升。请以管理员身份运行本程序。"));
        }
        return;
    }

    // ── Normal install ───────────────────────────────────────────────────
    navigateTo(3);   // progress page

    auto* engine = new InstallerEngine();
    auto* thread = new QThread(this);

    engine->loadPackage();
    engine->moveToThread(thread);

    connect(engine, &InstallerEngine::progressChanged,
            this,   &InstallWizard::onProgressChanged);
    connect(engine, &InstallerEngine::statusChanged,
            this,   &InstallWizard::onStatusChanged);
    connect(engine, &InstallerEngine::finished,
            this,   &InstallWizard::onFinished);

    connect(thread, &QThread::started, engine, [engine, installDir]() {
        engine->install(installDir);
    });
    connect(engine, &InstallerEngine::finished, thread,
            [thread, engine]() { engine->deleteLater(); thread->quit(); });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Uninstall action
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::startUninstallation()
{
    navigateTo(1);   // progress page

    auto* engine = new InstallerEngine();
    auto* thread = new QThread(this);

    engine->loadPackage();
    engine->moveToThread(thread);

    connect(engine, &InstallerEngine::progressChanged,
            this,   &InstallWizard::onProgressChanged);
    connect(engine, &InstallerEngine::statusChanged,
            this,   &InstallWizard::onStatusChanged);
    connect(engine, &InstallerEngine::finished,
            this,   &InstallWizard::onFinished);

    const QString dir = m_uninstallDir;
    connect(thread, &QThread::started, engine, [engine, dir]() {
        engine->uninstall(dir);
    });
    connect(engine, &InstallerEngine::finished, thread,
            [thread, engine]() { engine->deleteLater(); thread->quit(); });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    thread->start();
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Progress / Finish callbacks
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::onProgressChanged(int percent)
{
    if (m_progressBar) m_progressBar->setValue(percent);
}

void InstallWizard::onStatusChanged(const QString& text)
{
    if (m_statusLabel) m_statusLabel->setText(text);
}

void InstallWizard::onFinished(bool success, const QString& error)
{
    if (!success) {
        QMessageBox::critical(this,
            (m_mode == Install) ? tr("安装失败") : tr("卸载失败"), error);
        return;
    }

    // Auto-advance to finish page
    navigateTo(m_stack->count() - 1);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Done-event: launch app after install
// ═══════════════════════════════════════════════════════════════════════════════

void InstallWizard::done(int result)
{
    if (result == QDialog::Accepted && m_mode == Install && m_runCheckBox
        && m_runCheckBox->isChecked()) {
        const auto inst = m_package.value(QStringLiteral("installer")).toObject();
        const auto fa   = m_package.value(QStringLiteral("finish_actions")).toObject();
        const QString exe = QDir(m_pathEdit->text()).absoluteFilePath(
            inst.value(QStringLiteral("main_executable")).toString());
        const QString args = fa.value(QStringLiteral("run_arguments")).toString();
        if (QFile::exists(exe))
            QProcess::startDetached(exe, args.isEmpty() ? QStringList{}
                : QStringList{args}, m_pathEdit->text());
    }
    QDialog::done(result);
}
