#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "projectconfigdialog.h"
#include "languagemanager.h"

#include <QFileDialog>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QMessageBox>
#include <QFileInfo>
#include <QFile>
#include <QDirIterator>
#include <QApplication>
#include <QDateTime>
#include <QThread>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSettings>
#include <QShortcut>
#include <QVector>
#include <QTextCursor>
#include <QTextStream>
#include <QLineEdit>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_process(new QProcess(this))
    , m_configured(false)
    , m_currentOp(OpNone)
    , m_platform("rp2040")
    , m_toolchain()
{
    ui->setupUi(this);
    setWindowIcon(QIcon(":/Raspberry_Pi.png"));

    connect(ui->btnNewProject, &QPushButton::clicked, this, &MainWindow::onNewProject);
    connect(ui->btnOpenProject, &QPushButton::clicked, this, &MainWindow::onOpenProject);
    connect(ui->btnProjectConfig, &QPushButton::clicked, this, &MainWindow::onProjectConfig);
    connect(ui->btnConfigure, &QPushButton::clicked, this, &MainWindow::onConfigure);
    connect(ui->btnBuild, &QPushButton::clicked, this, &MainWindow::onBuild);
    connect(ui->btnClean, &QPushButton::clicked, this, &MainWindow::onClean);
    connect(ui->btnFlash, &QPushButton::clicked, this, &MainWindow::onFlash);
    connect(ui->btnAbout, &QPushButton::clicked, this, &MainWindow::onAbout);
    connect(ui->btnExit, &QPushButton::clicked, this, &MainWindow::onExit);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &MainWindow::onProcessOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &MainWindow::onProcessOutput);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &MainWindow::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &MainWindow::onProcessError);

    new QShortcut(QKeySequence(Qt::Key_F1), this, SLOT(onBuild()));
    new QShortcut(QKeySequence(Qt::Key_F2), this, SLOT(onFlash()));
    new QShortcut(QKeySequence(Qt::Key_F3), this, SLOT(onConfigure()));
    new QShortcut(QKeySequence(Qt::Key_F4), this, SLOT(onClean()));
    new QShortcut(QKeySequence(Qt::Key_F8), this, SLOT(onProjectConfig()));

    QMenu *langMenu = ui->menubar->addMenu("语言 (&L)");
    QAction *actChinese = langMenu->addAction("中文 (&Z)");
    QAction *actEnglish = langMenu->addAction("English (&E)");
    m_langMenu = langMenu;
    m_actChinese = actChinese;
    m_actEnglish = actEnglish;
    actChinese->setCheckable(true);
    actEnglish->setCheckable(true);
    actChinese->setChecked(LanguageManager::isChinese());
    actEnglish->setChecked(!LanguageManager::isChinese());
    connect(actChinese, &QAction::triggered, this, [this]() {
        LanguageManager::instance().setLanguage(LanguageManager::Chinese);
        saveConfig();
    });
    connect(actEnglish, &QAction::triggered, this, [this]() {
        LanguageManager::instance().setLanguage(LanguageManager::English);
        saveConfig();
    });
    connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, [this, actChinese, actEnglish]() {
        actChinese->setChecked(LanguageManager::isChinese());
        actEnglish->setChecked(!LanguageManager::isChinese());
        updateLanguage();
    });

    QString configPath = QApplication::applicationDirPath() + "/PicoBuild.ini";
    if (QFileInfo::exists(configPath)) {
        QSettings settings(configPath, QSettings::IniFormat);
        QString savedLang = settings.value("App/Language", "Chinese").toString();
        if (savedLang == "English") {
            LanguageManager::instance().setLanguage(LanguageManager::English);
        }
    }

    ui->btnBuild->setEnabled(false);
    ui->btnFlash->setEnabled(false);
    ui->statusbar->showMessage(LanguageManager::isChinese() ? "就绪" : "Ready");

    ui->textEditOutput->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->textEditOutput, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        bool zh = LanguageManager::isChinese();
        QMenu *menu = new QMenu(this);
        QAction *selectAllAction = menu->addAction(zh ? "全选 (Ctrl+A)" : "Select All (Ctrl+A)");
        QAction *copyAction = menu->addAction(zh ? "复制 (Ctrl+C)" : "Copy (Ctrl+C)");
        menu->addSeparator();
        QAction *clearAction = menu->addAction(zh ? "清空输出" : "Clear Output");
        connect(selectAllAction, &QAction::triggered, ui->textEditOutput, &QTextEdit::selectAll);
        connect(copyAction, &QAction::triggered, ui->textEditOutput, &QTextEdit::copy);
        connect(clearAction, &QAction::triggered, this, &MainWindow::onClearOutput);
        menu->exec(ui->textEditOutput->mapToGlobal(pos));
        menu->deleteLater();
    });

    appendOutput(LanguageManager::isChinese() ? "PicoBuild 已启动。使用「新建工程」或「打开工程」开始。"
                                              : "PicoBuild started. Use \"New Project\" or \"Open Project\" to begin.", QColor("#4ec9b0"));
    loadConfig();
}

MainWindow::~MainWindow()
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }
    delete ui;
}

QString MainWindow::sourceDir() const
{
    return m_sourceDir;
}

QString MainWindow::sdkPath() const
{
    return m_sdkPath;
}

void MainWindow::setSdkPath(const QString &path)
{
    m_sdkPath = path;
    m_sdkBaseDir = QDir::fromNativeSeparators(path);
    scanSdkInternal();
}

QString MainWindow::autoDetectSdk()
{
    QStringList searchPaths;

    QString appDir = QApplication::applicationDirPath();
    searchPaths << appDir + "/.pico-sdk"
                << appDir + "/../.pico-sdk"
                << appDir + "/../../.pico-sdk"
                << QDir::homePath() + "/.pico-sdk";

    if (!m_sourceDir.isEmpty()) {
        searchPaths << m_sourceDir + "/.pico-sdk"
                    << m_sourceDir + "/../.pico-sdk"
                    << m_sourceDir + "/../../.pico-sdk";
    }

    for (const QString &path : searchPaths) {
        QString cleanPath = QDir::cleanPath(path);
        if (QFileInfo::exists(cleanPath + "/sdk")) {
            return QDir::toNativeSeparators(cleanPath);
        }
    }

    return QString();
}

void MainWindow::scanSdkInternal()
{
    QString sdkDir = m_sdkBaseDir;
    if (sdkDir.isEmpty()) {
        sdkDir = autoDetectSdk();
        if (!sdkDir.isEmpty()) {
            m_sdkPath = QDir::toNativeSeparators(sdkDir);
            m_sdkBaseDir = sdkDir;
        } else {
            appendOutput(LanguageManager::isChinese() ? "SDK 扫描: 未找到 .pico-sdk 目录" : "SDK Scan: .pico-sdk directory not found", QColor("#f44747"));
            return;
        }
    }

    m_cmakePath = findLatestVersion(m_sdkBaseDir + "/cmake", "*/bin/cmake.exe");
    m_ninjaPath = findLatestVersion(m_sdkBaseDir + "/ninja", "*/ninja.exe");
    m_picoSdkPath = findLatestVersion(m_sdkBaseDir + "/sdk", "*");
    m_picotoolPath = findLatestVersion(m_sdkBaseDir + "/picotool", "*/picotool/picotool.exe");
    if (m_picotoolPath.isEmpty()) {
        QDir ptDir(m_sdkBaseDir + "/picotool");
        if (ptDir.exists()) {
            QStringList ptVers = ptDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
            for (const QString &v : ptVers) {
                QString exe = m_sdkBaseDir + "/picotool/" + v + "/picotool/picotool.exe";
                if (QFileInfo::exists(exe)) {
                    m_picotoolPath = QDir::toNativeSeparators(exe);
                    break;
                }
            }
        }
    }

    m_toolchainPath.clear();
    m_riscvToolchainPath.clear();
    QDir tcDir(m_sdkBaseDir + "/toolchain");
    if (tcDir.exists()) {
        QStringList tcEntries = tcDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
        for (const QString &entry : tcEntries) {
            QString candidate = m_sdkBaseDir + "/toolchain/" + entry;
            if (m_toolchainPath.isEmpty() && QFileInfo::exists(candidate + "/bin/arm-none-eabi-gcc.exe")) {
                m_toolchainPath = QDir::toNativeSeparators(candidate);
            }
            if (m_riscvToolchainPath.isEmpty() && QFileInfo::exists(candidate + "/bin/riscv32-unknown-elf-gcc.exe")) {
                m_riscvToolchainPath = QDir::toNativeSeparators(candidate);
            }
        }
    }

    m_pioasmPath = findLatestVersion(m_sdkBaseDir + "/tools", "*/pioasm/pioasm.exe");

    QString pythonDir = QDir::homePath() + "/.pico-sdk/python";
    m_pythonExe = findLatestVersion(pythonDir, "*/python.exe");

    bool zh = LanguageManager::isChinese();

    appendOutput("", QColor("#555555"));
    appendOutput(QString(50, QChar(0x2500)), QColor("#569cd6"), true);
    appendOutput(zh ? "  SDK 扫描" : "  SDK Scan", QColor("#ffcc00"), true, "14px");
    appendOutput(QString(50, QChar(0x2500)), QColor("#569cd6"), true);
    appendOutput((zh ? "SDK 根目录: " : "SDK Root: ") + QDir::toNativeSeparators(m_sdkBaseDir), QColor("#9cdcfe"));

    QStringList statusParts;
    if (!m_cmakePath.isEmpty()) {
        appendOutput("CMake:     " + m_cmakePath, QColor("#6a9955"));
        statusParts << "CMake OK";
    } else {
        appendOutput("CMake:     " + QString(zh ? "未找到" : "Not Found"), QColor("#f44747"));
        statusParts << "CMake MISSING";
    }

    if (!m_ninjaPath.isEmpty()) {
        appendOutput("Ninja:      " + m_ninjaPath, QColor("#6a9955"));
    } else {
        appendOutput("Ninja:      " + QString(zh ? "未找到" : "Not Found"), QColor("#f44747"));
    }

    if (!m_picoSdkPath.isEmpty()) {
        appendOutput("Pico SDK:   " + m_picoSdkPath, QColor("#6a9955"));
        statusParts << "SDK OK";
    } else {
        appendOutput("Pico SDK:   " + QString(zh ? "未找到" : "Not Found"), QColor("#f44747"));
    }

    if (!m_toolchainPath.isEmpty()) {
        appendOutput((zh ? "ARM工具链:  " : "ARM GCC:    ") + m_toolchainPath, QColor("#6a9955"));
        statusParts << "ARM GCC OK";
    } else {
        appendOutput((zh ? "ARM工具链:  " : "ARM GCC:    ") + QString(zh ? "未找到" : "Not Found"), QColor("#f44747"));
    }

    if (!m_riscvToolchainPath.isEmpty()) {
        appendOutput((zh ? "RISC-V工具链: " : "RISC-V GCC:  ") + m_riscvToolchainPath, QColor("#6a9955"));
        statusParts << "RISC-V GCC OK";
    } else {
        appendOutput((zh ? "RISC-V工具链: " : "RISC-V GCC:  ") + QString(zh ? "未找到" : "Not Found"), QColor("#ce9178"));
    }

    if (!m_pioasmPath.isEmpty()) {
        appendOutput("Pioasm:      " + m_pioasmPath, QColor("#6a9955"));
    } else {
        appendOutput("Pioasm:      " + QString(zh ? "未找到" : "Not Found"), QColor("#ce9178"));
    }

    if (!m_pythonExe.isEmpty()) {
        appendOutput("Python3:    " + m_pythonExe, QColor("#6a9955"));
    } else {
        appendOutput("Python3:    " + QString(zh ? "未找到" : "Not Found"), QColor("#f44747"));
    }

    if (!m_picotoolPath.isEmpty()) {
        appendOutput("Picotool:   " + m_picotoolPath, QColor("#6a9955"));
    } else {
        appendOutput("Picotool:   " + QString(zh ? "未找到" : "Not Found"), QColor("#f44747"));
    }

    if (!m_cmakePath.isEmpty() && !m_ninjaPath.isEmpty()
        && !m_picoSdkPath.isEmpty() && !m_toolchainPath.isEmpty()) {
        appendOutput("", QColor("#555555"));
        appendOutput(zh ? "  SDK 就绪 - 所有工具链已找到" : "  SDK Ready - All toolchains found", QColor("#4ec9b0"), true, "13px");
        appendOutput("", QColor("#555555"));
        ui->statusbar->setStyleSheet("color: #4ec9b0; font-weight: bold;");
        ui->statusbar->showMessage(zh ? "SDK 就绪" : "SDK Ready");
    } else {
        appendOutput("", QColor("#555555"));
        appendOutput(zh ? "  SDK 不完整 - 部分工具未找到，请检查 SDK 目录" : "  SDK Incomplete - Some tools not found, check SDK directory", QColor("#ff4444"), true, "13px");
        appendOutput("", QColor("#555555"));
        ui->statusbar->setStyleSheet("color: #ff4444; font-weight: bold;");
        ui->statusbar->showMessage(QString(zh ? "SDK 不完整" : "SDK Incomplete") + "  |  " + statusParts.join("  |  "));
    }

    saveConfig();
}

void MainWindow::openProject(const QString &sourceDir)
{
    m_sourceDir = QDir::fromNativeSeparators(sourceDir);

    QString sdkDetected = autoDetectSdk();
    if (!sdkDetected.isEmpty()) {
        m_sdkPath = sdkDetected;
        m_sdkBaseDir = QDir::fromNativeSeparators(sdkDetected);
        scanSdkInternal();
    }

    updateProjectDisplay();
    saveConfig();
}

void MainWindow::updateProjectDisplay()
{
    bool zh = LanguageManager::isChinese();

    if (m_sourceDir.isEmpty()) {
    ui->labelSourceDir->setText(zh ? "未打开工程" : "No Project Open");
    ui->labelCurrentProject->setText("");
    ui->labelPlatformValue->setText("-");
        return;
    }

    QFileInfo fi(m_sourceDir);
    QString projectName = fi.fileName();

    QString displayName;
    if (m_platform == "rp2040") displayName = "RP2040";
    else if (m_platform == "rp2350-arm-s") displayName = "RP2350 ARM";
    else if (m_platform == "rp2350-riscv") displayName = "RP2350 RISC-V";
    else displayName = m_platform;

    ui->labelSourceDir->setText("[" + projectName + "] -> " + QDir::toNativeSeparators(m_sourceDir));
    ui->labelCurrentProject->setText("");
    ui->labelPlatformValue->setText(displayName);
}

QString MainWindow::platformValue() const
{
    return m_platform;
}

void MainWindow::onNewProject()
{
    bool zh = LanguageManager::isChinese();
    QDialog dlg(this);
    dlg.setWindowTitle(zh ? "新建工程" : "New Project");
    dlg.setFixedSize(440, 260);
    dlg.setStyleSheet("QDialog { background-color: #2d2d2d; } QLabel { color: #555555; font-size: 13px; }");

    QVBoxLayout *layout = new QVBoxLayout(&dlg);
    layout->setSpacing(12);
    layout->setContentsMargins(20, 16, 20, 16);

    QLabel *title = new QLabel(zh ? "创建新的 Pico 工程" : "Create New Pico Project");
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: #569cd6;");
    layout->addWidget(title);

    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(8);

    QLabel *lblName = new QLabel(zh ? "工程名称:" : "Project Name:");
    QLineEdit *editName = new QLineEdit();
    editName->setPlaceholderText(zh ? "例如: my_project" : "e.g. my_project");
    editName->setStyleSheet("background-color: #3a3a3a; color: #e0e0e0; border: 1px solid #505050; border-radius: 4px; padding: 6px 10px;");

    QLabel *lblLoc = new QLabel(zh ? "保存位置:" : "Save Location:");
    QLineEdit *editLoc = new QLineEdit();
    editLoc->setPlaceholderText(zh ? "选择工程存放目录..." : "Select project save location...");
    editLoc->setStyleSheet("background-color: #3a3a3a; color: #e0e0e0; border: 1px solid #505050; border-radius: 4px; padding: 6px 10px;");
    QString defaultLoc = QDir::homePath() + "/PicoProjects";
    editLoc->setText(QDir::toNativeSeparators(defaultLoc));

    QPushButton *btnBrowseLoc = new QPushButton(zh ? "浏览..." : "Browse...");
    btnBrowseLoc->setMaximumWidth(80);
    btnBrowseLoc->setStyleSheet("QPushButton { background-color: #3a3a3a; color: #555555; border-radius: 4px; padding: 6px 10px; } QPushButton:hover { background-color: #505050; }");

    QLabel *lblMcu = new QLabel(zh ? "MCU:" : "MCU:");
    QComboBox *comboMcu = new QComboBox();
    comboMcu->addItem("RP2040 (Pico)");
    comboMcu->addItem("RP2350 ARM (Pico2 ARM)");
    comboMcu->addItem("RP2350 RISC-V (Pico2 RISC-V)");
    comboMcu->setStyleSheet("QComboBox { background-color: #3a3a3a; color: #e0e0e0; border: 1px solid #505050; border-radius: 4px; padding: 6px 10px; min-width: 180px; } QComboBox QAbstractItemView { background-color: #3a3a3a; color: #e0e0e0; selection-background-color: #0078d4; }");

    grid->addWidget(lblName, 0, 0);
    grid->addWidget(editName, 0, 1, 1, 2);
    grid->addWidget(lblLoc, 1, 0);
    grid->addWidget(editLoc, 1, 1);
    grid->addWidget(btnBrowseLoc, 1, 2);
    grid->addWidget(lblMcu, 2, 0);
    grid->addWidget(comboMcu, 2, 1, 1, 2);

    layout->addLayout(grid);
    layout->addStretch();

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *btnCancel = new QPushButton(zh ? "取消" : "Cancel");
    btnCancel->setStyleSheet("QPushButton { background-color: #3a3a3a; color: #555555; border-radius: 4px; padding: 8px 20px; font-size: 13px; } QPushButton:hover { background-color: #505050; }");
    QPushButton *btnCreate = new QPushButton(zh ? "创建工程" : "Create Project");
    btnCreate->setStyleSheet("QPushButton { background-color: #107c10; color: white; font-weight: bold; border-radius: 4px; padding: 8px 20px; font-size: 13px; } QPushButton:hover { background-color: #1a8c1a; }");
    btnLayout->addWidget(btnCancel);
    btnLayout->addWidget(btnCreate);
    layout->addLayout(btnLayout);

    connect(btnBrowseLoc, &QPushButton::clicked, [&]() {
        QString dir = QFileDialog::getExistingDirectory(&dlg, zh ? "选择工程存放目录" : "Select Project Save Location", editLoc->text());
        if (!dir.isEmpty()) editLoc->setText(QDir::toNativeSeparators(dir));
    });
    connect(btnCancel, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(btnCreate, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() != QDialog::Accepted) return;

    QString projectName = editName->text().trimmed();
    QString location = editLoc->text().trimmed();
    int mcuIndex = comboMcu->currentIndex();

    if (projectName.isEmpty()) {
        QMessageBox::warning(this, zh ? "输入错误" : "Input Error", zh ? "请输入工程名称！" : "Please enter a project name!");
        return;
    }

    if (location.isEmpty()) {
        QMessageBox::warning(this, zh ? "输入错误" : "Input Error", zh ? "请选择保存位置！" : "Please select a save location!");
        return;
    }

    QString projectDir = QDir::fromNativeSeparators(location) + "/" + projectName;
    QDir dir;
    if (dir.exists(projectDir)) {
        QMessageBox::warning(this, zh ? "创建失败" : "Creation Failed",
                             QString(zh ? "目录已存在：%1\n请选择其他名称或位置。" : "Directory already exists: %1\nPlease choose a different name or location.").arg(projectDir));
        return;
    }
    if (!dir.mkpath(projectDir)) {
        QMessageBox::critical(this, zh ? "创建失败" : "Creation Failed", QString(zh ? "无法创建目录：%1" : "Cannot create directory: %1").arg(projectDir));
        return;
    }

    QString platform;
    if (mcuIndex == 0) platform = "rp2040";
    else if (mcuIndex == 1) platform = "rp2350-arm-s";
    else platform = "rp2350-riscv";

    QString board = platform.startsWith("rp2350") ? "pico2" : "pico";

    // Set default toolchain based on platform
    m_toolchain = (platform == "rp2040") ? "pico_arm_cortex_m0plus_gcc" :
                  (platform == "rp2350-arm-s") ? "pico_arm_cortex_m33_gcc" :
                  (platform == "rp2350-riscv") ? "pico_riscv_gcc_zcb_zcmp" : "";

    QString cmakeContent = QString(
        "cmake_minimum_required(VERSION 3.13)\n"
        "include(pico_sdk_import.cmake)\n"
        "\n"
        "project(%1 C CXX ASM)\n"
        "set(CMAKE_C_STANDARD 11)\n"
        "set(CMAKE_CXX_STANDARD 17)\n"
        "\n"
        "pico_sdk_init()\n"
        "# PicoBuild-Toolchain: " + m_toolchain + "\n"
        "\n"
        "add_executable(%1\n"
        "    main.c\n"
        ")\n"
        "\n"
        "target_link_libraries(%1\n"
        "    pico_stdlib\n"
        ")\n"
        "\n"
        "pico_enable_stdio_usb(%1 1)\n"
        "pico_enable_stdio_uart(%1 0)\n"
        "pico_add_extra_outputs(%1)\n"
    ).arg(projectName);

    QString mainCContent = QString(
        "#include \"pico/stdlib.h\"\n"
        "\n"
        "int main()\n"
        "{\n"
        "    stdio_init_all();\n"
        "\n"
        "    while (true) {\n"
        "        // TODO: %1\n"
        "        sleep_ms(1000);\n"
        "    }\n"
        "    return 0;\n"
        "}\n"
    ).arg(platform.startsWith("rp2350") ? "Your RP2350 code here" : "Your RP2040 code here");

    QFile cmakeFile(projectDir + "/CMakeLists.txt");
    if (cmakeFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        cmakeFile.write(cmakeContent.toUtf8());
        cmakeFile.close();
    }

    QFile mainFile(projectDir + "/main.c");
    if (mainFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        mainFile.write(mainCContent.toUtf8());
        mainFile.close();
    }

    QString sdkPath = autoDetectSdk();
    QString sdkImportSrc;
    if (!sdkPath.isEmpty()) {
        sdkImportSrc = QDir::fromNativeSeparators(sdkPath) + "/sdk";
        QDir sdkDir(sdkImportSrc);
        QStringList versions = sdkDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
        if (!versions.isEmpty()) {
            sdkImportSrc += "/" + versions.first() + "/external/pico_sdk_import.cmake";
        }
    }
    if (!sdkImportSrc.isEmpty() && QFileInfo::exists(sdkImportSrc)) {
        QFile::copy(sdkImportSrc, projectDir + "/pico_sdk_import.cmake");
    } else {
        appendOutput(zh ? "警告: 未找到 pico_sdk_import.cmake，请手动复制到工程目录。" : "Warning: pico_sdk_import.cmake not found, please copy manually to project directory.", QColor("#e5c07b"));
    }

    m_platform = platform;
    openProject(projectDir);

    appendOutput("", QColor("#555555"));
    appendOutput(QString(50, QChar(0x2500)), QColor("#4ec9b0"), true);
    appendOutput(zh ? "  工程创建成功!" : "  Project Created!", QColor("#4ec9b0"), true, "14px");
    appendOutput(QString(50, QChar(0x2500)), QColor("#4ec9b0"), true);
    appendOutput((zh ? "工程目录: " : "Project Dir: ") + QDir::toNativeSeparators(projectDir), QColor("#9cdcfe"));
    appendOutput("MCU: " + platform + (zh ? "    开发板: " : "    Board: ") + board, QColor("#9cdcfe"));
    appendOutput("", QColor("#555555"));
}

void MainWindow::onOpenProject()
{
    bool zh = LanguageManager::isChinese();
    QString lastDir = m_sourceDir;
    if (lastDir.isEmpty()) lastDir = QDir::homePath();

    QString dir = QFileDialog::getExistingDirectory(this, zh ? "打开 Pico 工程目录" : "Open Pico Project Directory", lastDir);
    if (dir.isEmpty()) return;

    QString cmakeFile = QDir::fromNativeSeparators(dir) + "/CMakeLists.txt";
    if (!QFileInfo::exists(cmakeFile)) {
        QMessageBox::warning(this, zh ? "打开工程失败" : "Open Project Failed",
                             (zh ? "所选目录中未找到 CMakeLists.txt 文件！\n\n"
                                   "路径: " : "CMakeLists.txt not found in selected directory!\n\n"
                                   "Path: ") + dir + (zh ? "\n\n请确保选择的是 Pico 工程源码目录。" : "\n\nMake sure it's a Pico project source directory."));
        return;
    }

    QFile file(cmakeFile);
    QString mcu = "rp2040";
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QString content = QString::fromUtf8(file.readAll());
        file.close();

        QRegularExpression boardRe("PICO_BOARD[:\"\\s=]+([a-zA-Z0-9_-]+)");
        QRegularExpressionMatch m = boardRe.match(content);
        if (m.hasMatch() && m.captured(1).contains("pico2")) {
            mcu = "rp2350-arm-s";
        }

        QRegularExpression platRe("PICO_PLATFORM[:\"\\s=]+([a-zA-Z0-9_-]+)");
        QRegularExpressionMatch mp = platRe.match(content);
        if (mp.hasMatch()) {
            mcu = mp.captured(1);
        }

        // Parse toolchain from PicoBuild comment
        QRegularExpression tcRe("#\\s*PicoBuild-Toolchain:\\s*(\\S+)");
        QRegularExpressionMatch tcm = tcRe.match(content);
        if (tcm.hasMatch()) {
            m_toolchain = tcm.captured(1);
        } else {
            // Default toolchain based on platform
            m_toolchain = (mcu == "rp2040") ? "pico_arm_cortex_m0plus_gcc" :
                          (mcu == "rp2350-arm-s") ? "pico_arm_cortex_m33_gcc" :
                          (mcu == "rp2350-riscv") ? "pico_riscv_gcc_zcb_zcmp" : "";
        }
    }

    m_platform = mcu;

    openProject(dir);

    appendOutput("", QColor("#555555"));
    appendOutput(QString(50, QChar(0x2500)), QColor("#4ec9b0"), true);
    appendOutput(zh ? "  工程已打开" : "  Project Opened", QColor("#4ec9b0"), true, "14px");
    appendOutput(QString(50, QChar(0x2500)), QColor("#4ec9b0"), true);
    appendOutput((zh ? "工程目录: " : "Project Dir: ") + QDir::toNativeSeparators(dir), QColor("#9cdcfe"));
    appendOutput("MCU: " + mcu, QColor("#9cdcfe"));
    appendOutput("", QColor("#555555"));
}

void MainWindow::onProjectConfig()
{
    bool zh = LanguageManager::isChinese();
    if (m_sourceDir.isEmpty()) {
        QMessageBox::information(this, zh ? "提示" : "Info",
                                 zh ? "请先打开或新建一个工程！\n\n"
                                     "点击「打开工程」选择已有工程，\n"
                                     "或点击「新建工程」创建新工程。"
                                   : "Please open or create a project first!\n\n"
                                     "Click 'Open Project' to select an existing project,\n"
                                     "or click 'New Project' to create a new one.");
        return;
    }

    QString cmakePath = m_sourceDir + "/CMakeLists.txt";
    if (!QFileInfo::exists(cmakePath)) {
        QMessageBox::information(this, zh ? "提示" : "Info",
                                 zh ? "未找到 CMakeLists.txt 文件！\n\n"
                                     "路径: " + QDir::toNativeSeparators(cmakePath)
                                   : "CMakeLists.txt not found!\n\n"
                                     "Path: " + QDir::toNativeSeparators(cmakePath));
        return;
    }

    QString curSdkVer = m_picoSdkPath.section('/', -1);
    ProjectConfigDialog dlg(cmakePath, m_sdkPath, m_platform, m_toolchain, curSdkVer, this);
    if (dlg.exec() == QDialog::Accepted) {
        m_sdkPath = dlg.sdkPath();
        m_toolchain = dlg.toolchain();
        m_sdkBaseDir = QDir::fromNativeSeparators(m_sdkPath);
        m_picoSdkPath = m_sdkBaseDir + "/sdk/" + dlg.sdkVersion();
        if (!m_sdkPath.isEmpty()) {
            scanSdkInternal();
        }
        appendOutput(zh ? "工程配置已更新: " : "Project config updated: " + QDir::toNativeSeparators(cmakePath), QColor("#4ec9b0"), true);
        saveConfig();
    }
}

void MainWindow::onAbout()
{
    bool zh = LanguageManager::isChinese();

    QMessageBox aboutBox(this);
    aboutBox.setWindowTitle(zh ? "关于 PicoBuild" : "About PicoBuild");
    aboutBox.setIcon(QMessageBox::NoIcon);
    aboutBox.setStyleSheet(
        "QMessageBox { background-color: #ffffff; }"
        "QLabel { color: #1a1a1a; font-size: 14px; }"
        "QPushButton { background-color: #0078d4; color: white; font-weight: bold; font-size: 13px; "
        "border-radius: 4px; padding: 6px 24px; min-width: 80px; }"
        "QPushButton:hover { background-color: #1a8cd8; }");

    QString info;
    info += "<h3 style='color:#1a5276; font-size:16px; font-weight:bold; margin-bottom:6px;'>PicoBuild</h3>";
    info += "<p style='color:#1a1a1a; font-size:14px; font-weight:bold;'>Raspberry Pi Pico / Pico2 Embedded Development Tool</p>";
    info += "<hr style='border:1px solid #c0c0c0;'>";
    info += "<table style='color:#1a1a1a; font-size:14px; font-weight:bold;'>";
    info += "<tr><td style='color:#555;'>" + QString(zh ? "Version:" : "Version:") + "</td><td style='color:#1a5276;'>v2.1.5</td></tr>";
    info += "<tr><td style='color:#555;'>" + QString(zh ? "Updated:" : "Updated:") + "</td><td>2026-07-20</td></tr>";
    info += "<tr><td style='color:#555;'>" + QString(zh ? "Chip Support:" : "Chip Support:") + "</td><td>RP2040 / RP2350 (ARM & RISC-V)</td></tr>";
    info += "<tr><td style='color:#555;'>" + QString(zh ? "Build System:" : "Build System:") + "</td><td>CMake + Ninja</td></tr>";
    info += "<tr><td style='color:#555;'>" + QString(zh ? "SDK:" : "SDK:") + "</td><td>Pico SDK 2.2.0</td></tr>";
    info += "</table>";
    info += "<hr style='border:1px solid #c0c0c0;'>";
    info += "<p style='color:#555; font-size:14px; font-weight:bold;'>" + QString(zh ? "Features:" : "Features:") + "</p>";
    info += "<p style='color:#1a1a1a; font-size:14px; font-weight:bold;'>"
            + QString(zh ? "New Project / Open Project / CMake Config / Build / Flash<br>"
                           "Project Config / Library Sync / Auto Header Include"
                         : "New Project / Open Project / CMake Config / Build / Flash<br>"
                           "Project Config / Library Sync / Auto Header Include")
            + "</p>";
    info += "<hr style='border:1px solid #c0c0c0;'>";
    info += "<p style='color:#555; font-size:14px; font-weight:bold;'>" + QString(zh ? "Author:" : "Author:") + "</p>";
    info += "<p style='color:#b8860b; font-size:17px; font-weight:bold;'>@SSGMETAL</p>";
    info += "<p style='color:#888; font-size:12px; font-weight:bold;'>Copyright 2026. All rights reserved.</p>";

    aboutBox.setText(info);
    aboutBox.exec();
}

void MainWindow::onExit()
{
    close();
}

void MainWindow::onConfigure()
{
    bool zh = LanguageManager::isChinese();
    if (m_sdkBaseDir.isEmpty() || m_cmakePath.isEmpty()) {
        QString sdk = autoDetectSdk();
        if (!sdk.isEmpty()) {
            m_sdkPath = sdk;
            m_sdkBaseDir = QDir::fromNativeSeparators(sdk);
            scanSdkInternal();
        }
    }
    if (!validateInputs()) return;

    QString buildDir = m_sourceDir + "/build";
    QDir().mkpath(buildDir);

    QString cacheFile = buildDir + "/CMakeCache.txt";
    if (QFileInfo::exists(cacheFile)) {
        QFile::remove(cacheFile);
        appendOutput(zh ? "已清除旧的 CMake 缓存，确保工具链路径正确。" : "Cleared old CMake cache to ensure correct toolchain path.", QColor("#ce9178"));
    }

    QString platform = platformValue();
    QString board = platform.startsWith("rp2350") ? "pico2" : "pico";
    QString buildType = ui->comboBuildType->currentText();
    QString picoCompiler = m_toolchain;

    if (picoCompiler.isEmpty()) {
        picoCompiler = (platform == "rp2040") ? "pico_arm_cortex_m0plus_gcc" :
                       (platform == "rp2350-arm-s") ? "pico_arm_cortex_m33_gcc" :
                       (platform == "rp2350-riscv") ? "pico_riscv_gcc_zcb_zcmp" : "";
    }

    if (picoCompiler.isEmpty()) {
        QMessageBox::warning(this, zh ? "工具链错误" : "Toolchain Error",
                             zh ? "未选择工具链类型！\n\n请先通过「工程配置」选择工具链。"
                                : "No toolchain selected!\n\nPlease select a toolchain in Project Config.");
        return;
    }

    QString activeToolchain;
    if (picoCompiler.contains("riscv") || picoCompiler.contains("rv32"))
        activeToolchain = m_riscvToolchainPath;
    else
        activeToolchain = m_toolchainPath;

    if (activeToolchain.isEmpty()) {
        QString toolchainName = (picoCompiler.contains("riscv") || picoCompiler.contains("rv32")) ? "RISC-V" : "ARM";
        QMessageBox::warning(this, zh ? "工具链错误" : "Toolchain Error",
                             QString(zh ? "未找到 %1 工具链！\n\n请检查 SDK 目录中的 toolchain 文件夹。"
                                        : "%1 toolchain not found!\n\nPlease check the toolchain folder in SDK directory.").arg(toolchainName));
        return;
    }

    QString compilerExe = (picoCompiler.contains("riscv") || picoCompiler.contains("rv32"))
        ? activeToolchain + "/bin/riscv32-unknown-elf-gcc.exe"
        : activeToolchain + "/bin/arm-none-eabi-gcc.exe";

    QStringList args;
    args << "-S" << m_sourceDir
         << "-B" << buildDir
         << "-G" << "Ninja"
         << "-DCMAKE_MAKE_PROGRAM:FILEPATH=" + m_ninjaPath
         << "-DCMAKE_BUILD_TYPE:STRING=" + buildType
         << "-DPICO_SDK_PATH:FILEPATH=" + m_picoSdkPath
         << "-DPICO_BOARD:STRING=" + board
         << "-DPICO_PLATFORM:STRING=" + platform
         << "-DPICO_COMPILER:STRING=" + picoCompiler
         << "-DPICO_TOOLCHAIN_PATH:FILEPATH=" + activeToolchain;

    if (!m_pythonExe.isEmpty()) {
        args << "-DPython3_EXECUTABLE:FILEPATH=" + m_pythonExe;
    }

    if (!m_picotoolPath.isEmpty()) {
        QString picotoolDir = QFileInfo(m_picotoolPath).absolutePath();
        args << "-Dpicotool_DIR=" + QDir::toNativeSeparators(picotoolDir);
    } else {
        args << "-DPICOTOOL_FETCH_FROM_GIT=OFF";
        appendOutput(zh ? "  picotool 未找到，已禁用在线下载 (需 git)" : "  picotool not found, disabled online download (needs git)", QColor("#ce9178"));
    }

    if (!m_pioasmPath.isEmpty()) {
        QString pioasmDir = QFileInfo(m_pioasmPath).absolutePath();
        args << "-Dpioasm_DIR=" + QDir::toNativeSeparators(pioasmDir);
    }

    appendOutput("", QColor("#555555"));
    appendOutput(QString(50, QChar(0x2500)), QColor("#569cd6"), true);
    appendOutput(zh ? "  CMake 配置" : "  CMake Configure", QColor("#ffcc00"), true, "14px");
    appendOutput(QString(50, QChar(0x2500)), QColor("#569cd6"), true);

    highlightOutput(zh ? " 芯片配置" : " Chip Config",
        QStringList() << (zh ? "MCU: " : "MCU: ") + platform + (zh ? "    开发板: " : "    Board: ") + board + (zh ? "    构建类型: " : "    Build Type: ") + buildType);

    appendOutput((zh ? "源码目录: " : "Source Dir: ") + m_sourceDir, QColor("#9cdcfe"));
    appendOutput((zh ? "构建目录: " : "Build Dir: ") + buildDir, QColor("#9cdcfe"));
    appendOutput((zh ? "SDK 路径:   " : "SDK Path:   ") + m_picoSdkPath, QColor("#9cdcfe"));
    appendOutput((zh ? "工具链:     " : "Toolchain:  ") + activeToolchain, QColor("#9cdcfe"));
    appendOutput((zh ? "编译器:     " : "Compiler:   ") + compilerExe, QColor("#9cdcfe"));
    if (!m_pythonExe.isEmpty()) {
        appendOutput("Python3:    " + m_pythonExe, QColor("#9cdcfe"));
    }
    appendOutput("", QColor("#555555"));

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PICO_TOOLCHAIN_PATH", QDir::toNativeSeparators(activeToolchain));
    QString toolchainBin = QDir::toNativeSeparators(activeToolchain + "/bin");
    QString currentPath = env.value("PATH");
    env.insert("PATH", toolchainBin + ";" + currentPath);

    if (!m_pythonExe.isEmpty()) {
        QFileInfo pyInfo(m_pythonExe);
        QString pyBinDir = QDir::toNativeSeparators(pyInfo.absolutePath());
        env.insert("PATH", pyBinDir + ";" + env.value("PATH"));
    }

    m_configured = false;
    m_currentOp = OpConfigure;
    setButtonsEnabled(false);
    runCommand(m_cmakePath, args, QString(), env);
}

void MainWindow::onBuild()
{
    bool zh = LanguageManager::isChinese();
    m_memoryLines.clear();

    QString buildDir = m_sourceDir + "/build";

    QStringList args;
    args << "--build" << buildDir;

    QString buildType = ui->comboBuildType->currentText();
    args << "--config" << buildType;

    appendOutput("", QColor("#555555"));
    appendOutput(QString(50, QChar(0x2500)), QColor("#569cd6"), true);
    appendOutput(zh ? "  编译" : "  Build", QColor("#ffcc00"), true, "14px");
    appendOutput(QString(50, QChar(0x2500)), QColor("#569cd6"), true);
    appendOutput((zh ? "构建目录: " : "Build Dir: ") + buildDir, QColor("#9cdcfe"));
    appendOutput("", QColor("#555555"));

    m_currentOp = OpBuild;
    setButtonsEnabled(false);
    runCommand(m_cmakePath, args);
}

void MainWindow::onClean()
{
    QString buildDir = m_sourceDir + "/build";
    bool zh = LanguageManager::isChinese();

    appendOutput("", QColor("#555555"));
    appendOutput(QString(50, QChar(0x2500)), QColor("#569cd6"), true);
    appendOutput(zh ? "  清理构建" : "  Clean Build", QColor("#ffcc00"), true, "14px");
    appendOutput(QString(50, QChar(0x2500)), QColor("#569cd6"), true);
    appendOutput("", QColor("#555555"));

    if (!QFileInfo::exists(buildDir)) {
        appendOutput(zh ? "构建目录不存在，无需清理。" : "Build directory does not exist, nothing to clean.", QColor("#ce9178"));
        return;
    }

    bool removed = QDir(buildDir).removeRecursively();
    if (removed) {
        appendOutput((zh ? "构建目录已彻底清除: " : "Build directory removed: ") + QDir::toNativeSeparators(buildDir), QColor("#4ec9b0"));
        m_configured = false;
        ui->btnBuild->setEnabled(false);
        ui->btnFlash->setEnabled(false);
    } else {
        appendOutput((zh ? "清理失败，请手动删除: " : "Clean failed, please delete manually: ") + QDir::toNativeSeparators(buildDir), QColor("#f44747"));
    }
}

QString MainWindow::findElfFile(const QString &buildDir) const
{
    QDirIterator it(buildDir, QStringList() << "*.elf", QDir::Files,
                    QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString path = it.next();
        if (!path.contains("ELF2UF2") && !path.contains("Pioasm") && !path.contains("bs2_default")) {
            return path;
        }
    }
    return QString();
}

void MainWindow::onFlash()
{
    QString buildDir = m_sourceDir + "/build";

    QString elfFile = findElfFile(buildDir);
    bool zh = LanguageManager::isChinese();
    if (elfFile.isEmpty()) {
        QMessageBox::warning(this, zh ? "烧录错误" : "Flash Error",
                             zh ? "未找到 .elf 固件文件！\n请先编译项目。" : "ELF firmware file not found!\nPlease build the project first.");
        return;
    }

    if (!QFileInfo::exists(m_picotoolPath)) {
        QMessageBox::warning(this, zh ? "烧录错误" : "Flash Error",
                             zh ? "未找到 picotool 工具！\n请确保 SDK 扫描正确。" : "Picotool not found!\nPlease ensure SDK scan is correct.");
        return;
    }

    QStringList args;
    args << "load" << QDir::toNativeSeparators(elfFile) << "-f";

    appendOutput("", QColor("#555555"));
    appendOutput(QString(50, QChar(0x2500)), QColor("#569cd6"), true);
    appendOutput(zh ? "  烧录固件" : "  Flash Firmware", QColor("#ffcc00"), true, "14px");
    appendOutput(QString(50, QChar(0x2500)), QColor("#569cd6"), true);
    appendOutput((zh ? "固件: " : "Firmware: ") + elfFile, QColor("#9cdcfe"));
    appendOutput("", QColor("#555555"));
    appendOutput(zh ? "提示: 请确保 Pico 已进入 BOOTSEL 模式（按住 BOOTSEL 按钮插入 USB）" : "Tip: Enter BOOTSEL mode (hold BOOTSEL while plugging USB)", QColor("#ce9178"), true);
    appendOutput("", QColor("#555555"));

    m_currentOp = OpFlash;
    setButtonsEnabled(false);
    runCommand(m_picotoolPath, args);
}

void MainWindow::loadConfig()
{
    QString configPath = QApplication::applicationDirPath() + "/PicoBuild.ini";
    if (!QFileInfo::exists(configPath)) return;

    QSettings settings(configPath, QSettings::IniFormat);
    QString sourceDir = settings.value("Paths/SourceDir").toString();
    QString sdkDir = settings.value("Paths/SdkDir").toString();
    QString platform = settings.value("Paths/Platform", "rp2040").toString();
    QString toolchain = settings.value("Paths/Toolchain").toString();
    QString buildType = settings.value("Paths/BuildType", "Release").toString();

    bool zh = LanguageManager::isChinese();
    if (!sourceDir.isEmpty() && QFileInfo::exists(sourceDir)) {
        m_sourceDir = QDir::fromNativeSeparators(sourceDir);
        appendOutput((zh ? "已加载源码目录: " : "Loaded source dir: ") + sourceDir, QColor("#6a9955"));
    }
    if (!sdkDir.isEmpty() && QFileInfo::exists(sdkDir)) {
        m_sdkPath = sdkDir;
        m_sdkBaseDir = QDir::fromNativeSeparators(sdkDir);
        appendOutput((zh ? "已加载 SDK 路径: " : "Loaded SDK path: ") + sdkDir, QColor("#6a9955"));
    }

    m_platform = platform;
    m_toolchain = toolchain;
    updateProjectDisplay();

    int btIdx = ui->comboBuildType->findText(buildType);
    if (btIdx >= 0) ui->comboBuildType->setCurrentIndex(btIdx);
}

void MainWindow::saveConfig()
{
    QString configPath = QApplication::applicationDirPath() + "/PicoBuild.ini";
    QSettings settings(configPath, QSettings::IniFormat);
    settings.setValue("Paths/SourceDir", QDir::toNativeSeparators(m_sourceDir));
    settings.setValue("Paths/SdkDir", m_sdkPath);
    settings.setValue("Paths/Platform", m_platform);
    settings.setValue("Paths/Toolchain", m_toolchain);
    settings.setValue("Paths/BuildType", ui->comboBuildType->currentText());
    settings.setValue("App/Language", LanguageManager::isChinese() ? "Chinese" : "English");
}

void MainWindow::updateLanguage()
{
    bool zh = LanguageManager::isChinese();

    ui->btnNewProject->setText(zh ? "新建工程" : "New Project");
    ui->btnOpenProject->setText(zh ? "打开工程" : "Open Project");
    ui->btnProjectConfig->setText(zh ? "工程配置" : "Project Config");
    ui->btnConfigure->setText(zh ? "配置 (F3)" : "Configure (F3)");
    ui->btnBuild->setText(zh ? "编译 (F1)" : "Build (F1)");
    ui->btnClean->setText(zh ? "清理 (F4)" : "Clean (F4)");
    ui->btnFlash->setText(zh ? "下载 (F2)" : "Flash (F2)");
    ui->btnAbout->setText(zh ? "关于" : "About");
    ui->btnExit->setText(zh ? "退出" : "Exit");

    ui->groupBoxMcpu->setTitle(zh ? "芯片配置" : "Chip Config");
    ui->labelPlatform->setText(zh ? "MCU:" : "MCU:");
    ui->labelBuildType->setText(zh ? "构建类型" : "Build Type");

    ui->labelCurrentProject->setText(zh ? "未打开工程" : "No Project Open");

    ui->statusbar->showMessage(zh ? "就绪" : "Ready");

    setWindowTitle(zh ? "PicoBuild - Raspberry Pi Pico MCU" : "PicoBuild - Raspberry Pi Pico MCU");

    if (m_langMenu) m_langMenu->setTitle(zh ? "语言 (&L)" : "Language (&L)");
    if (m_actChinese) m_actChinese->setText(zh ? "中文 (&Z)" : "Chinese (&Z)");
    if (m_actEnglish) m_actEnglish->setText(zh ? "English (&E)" : "English (&E)");

    updateProjectDisplay();
}

QString MainWindow::findLatestVersion(const QString &baseDir, const QString &pattern) const
{
    QDir dir(baseDir);
    if (!dir.exists()) return QString();

    QStringList entries = dir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
    QString fullPattern = baseDir + "/" + pattern;

    for (const QString &entry : entries) {
        QString candidate = baseDir + "/" + entry;
        if (pattern.endsWith(".exe") || pattern.endsWith("*")) {
            if (pattern == "*") {
                return QDir::toNativeSeparators(candidate);
            }
            QString subPath = candidate + "/" + pattern.mid(2);
            QFileInfo fi(subPath);
            if (fi.exists() && fi.isFile()) {
                return QDir::toNativeSeparators(subPath);
            }
            QDir subDir(candidate);
            QStringList subEntries = subDir.entryList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
                                                       QDir::Name | QDir::Reversed);
            for (const QString &subEntry : subEntries) {
                QString deeperPath = candidate + "/" + subEntry + "/" + pattern.mid(2);
                QFileInfo dfi(deeperPath);
                if (dfi.exists() && dfi.isFile()) {
                    return QDir::toNativeSeparators(deeperPath);
                }
            }
        }
    }
    return QString();
}

QString MainWindow::resolveToolPath(const QString &relativePath) const
{
    QString basePath = QApplication::applicationDirPath();
    QString fullPath = QDir::cleanPath(basePath + "/" + relativePath);

    if (QFileInfo::exists(fullPath)) {
        return QDir::toNativeSeparators(fullPath);
    }

    QDir dir(basePath);
    for (int i = 0; i < 5; ++i) {
        if (!dir.cdUp()) break;
        QString tryPath = QDir::cleanPath(dir.absolutePath() + "/" + relativePath);
        if (QFileInfo::exists(tryPath)) {
            return QDir::toNativeSeparators(tryPath);
        }
    }

    return QDir::toNativeSeparators(fullPath);
}

bool MainWindow::validateInputs()
{
    bool zh = LanguageManager::isChinese();

    if (m_sourceDir.isEmpty()) {
        QMessageBox::warning(this, zh ? "输入错误" : "Input Error",
                             zh ? "请先打开或新建 Pico 项目！" : "Please open or create a Pico project first!");
        return false;
    }
    QFileInfo srcInfo(m_sourceDir);
    if (!srcInfo.exists() || !srcInfo.isDir()) {
        QMessageBox::warning(this, zh ? "输入错误" : "Input Error",
                             zh ? "源码目录不存在！" : "Source directory does not exist!");
        return false;
    }

    QString cmakeFile = m_sourceDir + "/CMakeLists.txt";
    if (!QFileInfo::exists(cmakeFile)) {
        QMessageBox::warning(this, zh ? "输入错误" : "Input Error",
                             zh ? "所选目录中未找到 CMakeLists.txt 文件！\n请确保选择了正确的 Pico 项目源码目录。"
                                : "CMakeLists.txt not found in selected directory!\nPlease ensure it's a valid Pico project source directory.");
        return false;
    }

    if (m_cmakePath.isEmpty() || !QFileInfo::exists(m_cmakePath)) {
        QMessageBox::warning(this, zh ? "SDK 错误" : "SDK Error",
                             zh ? "未找到 CMake，请先在工程配置中手动设置 SDK 路径！"
                                : "CMake not found, please set SDK path in project config first!");
        return false;
    }
    return true;
}

void MainWindow::setButtonsEnabled(bool enabled)
{
    ui->btnConfigure->setEnabled(enabled);
    ui->btnBuild->setEnabled(enabled && m_configured);
    ui->btnClean->setEnabled(enabled);
    ui->btnFlash->setEnabled(enabled && m_configured);
    ui->btnProjectConfig->setEnabled(enabled);
    ui->btnNewProject->setEnabled(enabled);
    ui->btnOpenProject->setEnabled(enabled);
    if (!enabled) {
        m_hasProgressLine = false;
        ui->progressBar->setRange(0, 100);
        ui->progressBar->setValue(0);
    }
}

void MainWindow::appendOutput(const QString &text, const QColor &color, bool bold, const QString &fontSize, bool replaceLast)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString style = "color:" + color.name() + ";";
    if (bold) style += "font-weight:bold;";
    if (!fontSize.isEmpty()) style += "font-size:" + fontSize + ";";

    // For white background, use a slightly darker timestamp
    QString html = QString("<span style='color:#999;'>[%1]</span> "
                           "<span style='%2'>%3</span>")
                       .arg(timestamp)
                       .arg(style)
                       .arg(text.toHtmlEscaped().replace("\n", "<br>"));

    if (replaceLast) {
        QTextCursor cursor = ui->textEditOutput->textCursor();
        cursor.movePosition(QTextCursor::End);
        cursor.movePosition(QTextCursor::StartOfBlock, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertHtml(html);
    } else {
        ui->textEditOutput->append(html);
    }
}

void MainWindow::highlightOutput(const QString &title, const QStringList &items)
{
    appendOutput("", QColor("#888888"));
    appendOutput(QString(60, QChar(0x2500)), QColor("#2b6a9e"), true);
    appendOutput(title, QColor("#cc8800"), true, "14px");
    appendOutput(QString(60, QChar(0x2500)), QColor("#2b6a9e"), true);
    for (const QString &item : items) {
        appendOutput("  " + item, QColor("#222222"), true);
    }
    appendOutput(QString(60, QChar(0x2500)), QColor("#2b6a9e"), true);
    appendOutput("", QColor("#888888"));
}

void MainWindow::onClearOutput()
{
    ui->textEditOutput->clear();
}

void MainWindow::displayMemoryInfo()
{
    QString buildDir = m_sourceDir + "/build";
    QString elfPath = findElfFile(buildDir);

    if (elfPath.isEmpty()) return;

    QString sizeTool = m_toolchainPath + "/bin/arm-none-eabi-size.exe";
    if (!QFileInfo::exists(sizeTool)) return;

    QProcess sizeProc;
    sizeProc.start(sizeTool, QStringList() << "-A" << elfPath);
    if (!sizeProc.waitForFinished(5000)) return;

    QString output = QString::fromUtf8(sizeProc.readAllStandardOutput());
    if (output.isEmpty()) return;

    struct SecInfo {
        QString name;
        int size = 0;
    };
    QVector<SecInfo> sections;
    int totalSize = 0;

    static QRegularExpression secRe(R"(^\.(\S+)\s+(\d+)\s+)");
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        QRegularExpressionMatch m = secRe.match(line);
        if (m.hasMatch()) {
            QString name = m.captured(1);
            int size = m.captured(2).toInt();
            sections.append({name, size});
        }
        if (line.startsWith("Total")) {
            totalSize = line.section(QRegularExpression("\\s+"), 1, 1).toInt();
        }
    }

    int flashSize = 0;
    int ramSize = 0;
    for (const auto &s : sections) {
        QString n = s.name.toLower();
        if (n == "text" || n.contains("rodata") || n.contains("vector") || n.contains("init") || n.contains("fini") ||
            n.contains("got") || n.contains("eh_frame") || n.contains("jcr"))
            flashSize += s.size;
        else if (n == "data" || n == "bss" || n.contains("heap") || n.contains("stack") || n.contains("ram"))
            ramSize += s.size;
    }

    QString flashTotal = platformValue().contains("2350") ? "4 MB" : "2 MB";
    QString ramTotal = platformValue().contains("2350") ? "520 KB" : "264 KB";

    bool zh = LanguageManager::isChinese();

    appendOutput("", QColor("#555555"));
    appendOutput(QString(60, QChar(0x2500)), QColor("#ffcc00"), true);
    appendOutput(zh ? "  芯片资源占用" : "  Memory Usage", QColor("#ffcc00"), true, "14px");
    appendOutput(QString(60, QChar(0x2500)), QColor("#ffcc00"), true);

    QString header = QString(zh ? "  %1      %2            %3        %4"
                                : "  %1      %2            %3        %4")
        .arg(zh ? "区域" : "Region", -8).arg(zh ? "大小" : "Size", -12).arg(zh ? "总量" : "Total", -14).arg(zh ? "占用率" : "Usage", -8);
    appendOutput(header, QColor("#569cd6"), true);

    auto formatSize = [](int bytes) -> QString {
        if (bytes >= 1024 * 1024) return QString::number(bytes / (1024 * 1024.0), 'f', 2) + " MB";
        if (bytes >= 1024) return QString::number(bytes / 1024.0, 'f', 1) + " KB";
        return QString::number(bytes) + " B";
    };

    auto formatRow = [&](const QString &name, int used, const QString &totalStr) {
        double pct = totalSize > 0 ? used * 100.0 / totalSize : 0;
        QColor c = pct > 80.0 ? QColor("#ff4444") : QColor("#56b6c2");
        appendOutput(QString("  %1      %2            %3        %4%")
            .arg(name, -8).arg(formatSize(used), -12).arg(totalStr, -14).arg(QString::number(pct, 'f', 1), -8), c, true);
    };

    formatRow("FLASH", flashSize, flashTotal);
    formatRow("RAM", ramSize, ramTotal);
    appendOutput(QString(60, QChar(0x2500)), QColor("#ffcc00"), true);
    appendOutput("", QColor("#555555"));
}

void MainWindow::formatOutputLine(const QString &line, QColor &outColor, bool &outBold, QString &outFontSize)
{
    outColor = QColor("#444444");
    outBold = false;
    outFontSize = "";

    QString lower = line.toLower();

    if (line.contains("error:", Qt::CaseInsensitive) ||
        line.contains("fatal error", Qt::CaseInsensitive) ||
        (lower.contains("error") && lower.contains("failed"))) {
        outColor = QColor("#ff4444");
        outBold = true;
        return;
    }

    if (line.contains("FAILED", Qt::CaseSensitive) ||
        lower.contains("build failed") ||
        lower.contains("compilation terminated")) {
        outColor = QColor("#ff4444");
        outBold = true;
        outFontSize = "13px";
        return;
    }

    if (line.contains("warning:", Qt::CaseInsensitive) &&
        !line.contains("cmake warning", Qt::CaseInsensitive)) {
        outColor = QColor("#e5c07b");
        outBold = true;
        return;
    }

    if (lower.contains("memory region") ||
        lower.contains("flash") && (lower.contains("used") || lower.contains("total") || lower.contains("bytes") || lower.contains("size") || lower.contains("kb") || lower.contains("mb") || lower.contains("  b ") || lower.contains("%")) ||
        lower.contains("ram") && (lower.contains("used") || lower.contains("total") || lower.contains("bytes") || lower.contains("size") || lower.contains("kb") || lower.contains("mb") || lower.contains("  b ") || lower.contains("%")) ||
        lower.contains("sram") && (lower.contains("used") || lower.contains("total") || lower.contains("bytes") || lower.contains("size") || lower.contains("kb") || lower.contains("mb") || lower.contains("  b ") || lower.contains("%")) ||
        lower.contains("scratch") ||
        lower.contains("text") && (lower.contains("data") || lower.contains("bss")) && lower.contains("dec") && lower.contains("hex")) {
        outColor = QColor("#56b6c2");
        outBold = true;
        outFontSize = "13px";
        return;
    }

    if (lower.contains("text segment") ||
        lower.contains("data segment") ||
        lower.contains("bss segment") ||
        (lower.contains("firmware") && (lower.contains("size") || lower.contains("bytes")))) {
        outColor = QColor("#56b6c2");
        outBold = true;
        outFontSize = "13px";
        return;
    }

    if (line.contains("-- Configuring done") ||
        line.contains("-- Generating done") ||
        line.contains("-- Build files") ||
        lower.contains("build succeeded") ||
        lower.contains("build complete") ||
        lower.contains("linking cxx executable") ||
        lower.contains("linking c executable")) {
        outColor = QColor("#4ec9b0");
        outBold = true;
        return;
    }

    if (lower.contains("pico_board") ||
        lower.contains("pico_platform") ||
        lower.contains("pico_compiler") ||
        lower.contains("pico_sdk_path") ||
        lower.contains("target board") ||
        lower.contains("board configuration") ||
        lower.contains("using board") ||
        lower.contains("pico toolchain")) {
        outColor = QColor("#ffcc00");
        outBold = true;
        return;
    }

    if (line.contains("The C compiler identification") ||
        line.contains("The CXX compiler identification") ||
        line.contains("Detecting C compiler") ||
        line.contains("Detecting CXX compiler")) {
        outColor = QColor("#9cdcfe");
        outBold = true;
        return;
    }
}

void MainWindow::runCommand(const QString &program, const QStringList &arguments, const QString &workingDir, const QProcessEnvironment &env)
{
    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(3000);
    }

    QString cmdLine = program + " " + arguments.join(" ");
    appendOutput("执行: " + cmdLine, QColor("#569cd6"));

    if (env.isEmpty()) {
        m_process->setProcessEnvironment(QProcessEnvironment::systemEnvironment());
    } else {
        m_process->setProcessEnvironment(env);
    }

    if (!workingDir.isEmpty()) {
        m_process->setWorkingDirectory(workingDir);
    }
    m_process->start(program, arguments);
}

void MainWindow::onProcessOutput()
{
    static QRegularExpression buildProgress(R"(\[\d+/\d+\])");
    static QRegularExpression flashProgress(R"(Loading into Flash:)");

    auto isProgress = [&](const QString &line) -> bool {
        return buildProgress.match(line).hasMatch() || flashProgress.match(line).hasMatch();
    };

    auto isMemoryLine = [&](const QString &line) -> bool {
        if (isProgress(line)) return false;
        QString lower = line.toLower();
        return (lower.contains("memory region") ||
                (lower.contains("flash") && (lower.contains("used") || lower.contains("total") || lower.contains("bytes") || lower.contains("size") || lower.contains("kb") || lower.contains("mb") || lower.contains("  b ") || lower.contains("%"))) ||
                (lower.contains("ram") && (lower.contains("used") || lower.contains("total") || lower.contains("bytes") || lower.contains("size") || lower.contains("kb") || lower.contains("mb") || lower.contains("  b ") || lower.contains("%"))) ||
                (lower.contains("sram") && (lower.contains("used") || lower.contains("total") || lower.contains("bytes") || lower.contains("size") || lower.contains("kb") || lower.contains("mb") || lower.contains("  b ") || lower.contains("%"))) ||
                lower.contains("scratch") ||
                (lower.contains("text") && lower.contains("data") && lower.contains("bss") && lower.contains("dec") && lower.contains("hex")));
    };

    QByteArray data = m_process->readAllStandardOutput();
    if (!data.isEmpty()) {
        QString text = QString::fromUtf8(data).trimmed();
        QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            QColor color;
            bool bold;
            QString fontSize;
            formatOutputLine(line, color, bold, fontSize);
            bool thisIsProgress = isProgress(line);
            bool replace = thisIsProgress && m_hasProgressLine;
            appendOutput(line, color, bold, fontSize, replace);
            m_hasProgressLine = thisIsProgress;
            if (isMemoryLine(line)) {
                m_memoryLines.append(line);
            }
        }
        updateProgressFromText(text);
    }
    QByteArray errData = m_process->readAllStandardError();
    if (!errData.isEmpty()) {
        QString text = QString::fromUtf8(errData).trimmed();
        QStringList lines = text.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            QColor color;
            bool bold;
            QString fontSize;
            formatOutputLine(line, color, bold, fontSize);
            if (color == QColor("#555555") && !bold) {
                color = QColor("#6a9955");
            }
            bool thisIsProgress = isProgress(line);
            bool replace = thisIsProgress && m_hasProgressLine;
            appendOutput(line, color, bold, fontSize, replace);
            m_hasProgressLine = thisIsProgress;
            if (isMemoryLine(line)) {
                m_memoryLines.append(line);
            }
        }
        updateProgressFromText(text);
    }
}

void MainWindow::updateProgressFromText(const QString &text)
{
    static QRegularExpression ninjaProgress(R"(\[(\d+)/(\d+)\])");
    QRegularExpressionMatchIterator it = ninjaProgress.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int current = match.captured(1).toInt();
        int total = match.captured(2).toInt();
        if (total > 0) {
            int pct = current * 100 / total;
            ui->progressBar->setValue(pct);
        }
    }
}

void MainWindow::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    setButtonsEnabled(true);

    if (exitStatus == QProcess::CrashExit) {
        bool zh = LanguageManager::isChinese();
        appendOutput("", QColor("#555555"));
        appendOutput(QString(30, QChar(0x25A0)), QColor("#ff4444"), true, "16px");
        appendOutput(zh ? " 进程异常终止！" : " Process crashed!", QColor("#ff4444"), true, "16px");
        appendOutput(QString(30, QChar(0x25A0)), QColor("#ff4444"), true, "16px");
        QMessageBox::critical(this, zh ? "错误" : "Error", zh ? "进程异常终止！" : "Process crashed!");
        m_currentOp = OpNone;
        return;
    }

    if (exitCode == 0) {
        ui->progressBar->setValue(100);
        bool zh = LanguageManager::isChinese();
        QString opName;
        if (m_currentOp == OpConfigure) opName = zh ? "配置" : "Configure";
        else if (m_currentOp == OpBuild) opName = zh ? "编译" : "Build";
        else if (m_currentOp == OpClean) opName = zh ? "清理" : "Clean";
        else if (m_currentOp == OpFlash) opName = zh ? "烧录" : "Flash";
        else opName = zh ? "操作" : "Operation";

        appendOutput("", QColor("#555555"));
        appendOutput(QString(40, QChar(0x2500)), QColor("#4ec9b0"), true);
        appendOutput(QString("  %1 %2").arg(opName, zh ? "完成" : "Succeeded"),
            QColor("#4ec9b0"), true, "15px");
        appendOutput(QString(40, QChar(0x2500)), QColor("#4ec9b0"), true);
        if (m_currentOp == OpConfigure) {
            m_configured = true;
            ui->btnBuild->setEnabled(true);
            ui->btnFlash->setEnabled(true);
            QString buildDir = m_sourceDir + "/build";
            QString vscodeDir = m_sourceDir + "/.vscode";
            QDir().mkpath(vscodeDir);
            QProcess ninjaProc;
            ninjaProc.setWorkingDirectory(buildDir);
            QStringList ninjaArgs;
            ninjaArgs << "-t" << "compdb";
            ninjaProc.start(m_ninjaPath, ninjaArgs);
            if (ninjaProc.waitForFinished(5000)) {
                QByteArray output = ninjaProc.readAllStandardOutput();
                QFile f(vscodeDir + "/compile_commands.json");
                if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                    f.write(output);
                    f.close();
                }
            }
        }
        if (m_currentOp == OpBuild || m_currentOp == OpFlash) {
            displayMemoryInfo();
        }
    } else {
        bool zh = LanguageManager::isChinese();
        QString opName;
        if (m_currentOp == OpConfigure) opName = zh ? "配置" : "Configure";
        else if (m_currentOp == OpBuild) opName = zh ? "编译" : "Build";
        else if (m_currentOp == OpClean) opName = zh ? "清理" : "Clean";
        else if (m_currentOp == OpFlash) opName = zh ? "烧录" : "Flash";
        else opName = zh ? "操作" : "Operation";

        appendOutput("", QColor("#555555"));
        appendOutput(QString(40, QChar(0x25A0)), QColor("#ff4444"), true, "16px");
        appendOutput(QString("  %1 %2 (退出码: %3)").arg(opName, zh ? "失败" : "Failed").arg(exitCode),
            QColor("#ff4444"), true, "16px");
        appendOutput(QString(40, QChar(0x25A0)), QColor("#ff4444"), true, "16px");
        if (m_currentOp == OpConfigure) {
            m_configured = false;
            ui->btnBuild->setEnabled(false);
            ui->btnFlash->setEnabled(false);
        }
        QMessageBox::warning(this, zh ? "操作失败" : "Operation Failed",
                             QString(zh ? "操作失败，退出码: %1\n请查看输出日志了解详情。" : "Operation failed, exit code: %1\nCheck output log for details.").arg(exitCode));
    }

    m_currentOp = OpNone;
}

void MainWindow::onProcessError(QProcess::ProcessError error)
{
    setButtonsEnabled(true);
    m_currentOp = OpNone;

    bool zh = LanguageManager::isChinese();

    QString errorMsg;
    switch (error) {
    case QProcess::FailedToStart:
        errorMsg = zh ? "无法启动进程。请检查工具链是否完整安装。"
                      : "Failed to start process. Please check toolchain installation.";
        break;
    case QProcess::Timedout:
        errorMsg = zh ? "进程超时。" : "Process timed out.";
        break;
    default:
        errorMsg = zh ? "进程发生未知错误。" : "Unknown process error occurred.";
        break;
    }
    appendOutput("", QColor("#555555"));
    appendOutput(QString(30, QChar(0x25A0)), QColor("#ff4444"), true, "14px");
    appendOutput("  " + errorMsg, QColor("#ff4444"), true, "14px");
    appendOutput(QString(30, QChar(0x25A0)), QColor("#ff4444"), true, "14px");
    QMessageBox::critical(this, zh ? "错误" : "Error", errorMsg);
}