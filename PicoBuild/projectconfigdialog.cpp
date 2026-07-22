#include "projectconfigdialog.h"
#include "languagemanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFont>
#include <QRegularExpression>
#include <QMessageBox>
#include <QScrollArea>
#include <QGridLayout>
#include <QSplitter>

ProjectConfigDialog::ProjectConfigDialog(const QString &cmakePath, const QString &sdkPath,
                                          const QString &platform, const QString &toolchain,
                                          const QString &sdkVersion,
                                          QWidget *parent)
    : QDialog(parent)
    , m_cmakePath(cmakePath)
    , m_sdkPath(sdkPath)
    , m_platform(platform)
    , m_toolchain(toolchain)
    , m_sdkVersion(sdkVersion)
    , m_cStandard("11")
    , m_cxxStandard("17")
    , m_stdioUsb(false)
    , m_stdioUart(false)
    , m_extraOutputs(false)
{
    setWindowTitle(LanguageManager::isChinese() ? "工程配置 - " + QFileInfo(cmakePath).absolutePath() : "Project Config - " + QFileInfo(cmakePath).absolutePath());
    setMinimumSize(680, 620);
    resize(720, 680);

    parseCMakeLists();
    setupUI();
}

void ProjectConfigDialog::setupUI()
{
    bool zh = LanguageManager::isChinese();
    setStyleSheet(
        "QDialog { background-color: #ffffff; }"
        "QGroupBox { color: #1a5276; font-weight: bold; font-size: 13px; border: 1px solid #b0b0b0; border-radius: 6px; margin-top: 14px; padding-top: 10px; background-color: #f5f5f5; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; background-color: #f5f5f5; }"
        "QLabel { color: #1a1a1a; font-size: 13px; }"
        "QLineEdit { background-color: #ffffff; color: #1a1a1a; font-size: 13px; border: 1px solid #b0b0b0; border-radius: 4px; padding: 4px 8px; }"
        "QLineEdit:focus { border-color: #0078d4; }"
        "QComboBox { background-color: #ffffff; color: #1a1a1a; font-size: 13px; border: 1px solid #b0b0b0; border-radius: 4px; padding: 4px 8px; min-width: 100px; }"
        "QComboBox:hover { border-color: #0078d4; }"
        "QComboBox QAbstractItemView { background-color: #ffffff; color: #1a1a1a; font-size: 13px; selection-background-color: #0078d4; selection-color: white; }"
        "QListWidget { background-color: #ffffff; color: #1a1a1a; font-size: 13px; border: 1px solid #b0b0b0; border-radius: 4px; }"
        "QCheckBox { color: #1a1a1a; font-size: 13px; spacing: 6px; }"
        "QCheckBox::indicator { width: 16px; height: 16px; }"
        "QCheckBox::indicator:unchecked { border: 1px solid #b0b0b0; border-radius: 3px; background-color: #ffffff; }"
        "QCheckBox::indicator:checked { border: 1px solid #0078d4; border-radius: 3px; background-color: #0078d4; }"
        "QPushButton { font-size: 13px; }"
    );

    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QGroupBox *pathGroup = new QGroupBox(zh ? "SDK路径设置" : "SDK Path Settings");
    QGridLayout *pathLayout = new QGridLayout(pathGroup);
    pathLayout->addWidget(new QLabel(zh ? "SDK 路径:" : "SDK Path:"), 0, 0);
    m_sdkPathEdit = new QLineEdit(m_sdkPath);
    m_sdkPathEdit->setReadOnly(true);
    pathLayout->addWidget(m_sdkPathEdit, 0, 1);
    QPushButton *btnBrowseSdk = new QPushButton(zh ? "浏览..." : "Browse...");
    btnBrowseSdk->setMaximumWidth(80);
    btnBrowseSdk->setStyleSheet("QPushButton { background-color: #e0e0e0; color: #1a1a1a; border-radius: 4px; padding: 4px 10px; border: 1px solid #b0b0b0; } QPushButton:hover { background-color: #d0d0d0; }");
    connect(btnBrowseSdk, &QPushButton::clicked, [this, zh]() {
        QString dir = QFileDialog::getExistingDirectory(this, zh ? "选择 SDK 目录 (.pico-sdk)" : "Select SDK Directory (.pico-sdk)", m_sdkPathEdit->text());
        if (!dir.isEmpty()) m_sdkPathEdit->setText(QDir::toNativeSeparators(dir));
    });
    pathLayout->addWidget(btnBrowseSdk, 0, 2);

    QLabel *verLabel = new QLabel(zh ? "SDK 版本:" : "SDK Version:");
    QFont boldFont = verLabel->font();
    boldFont.setBold(true);
    verLabel->setFont(boldFont);
    pathLayout->addWidget(verLabel, 1, 0);
    m_sdkVersionCombo = new QComboBox;
    m_sdkVersionCombo->setMinimumWidth(150);
    QDir sdkDir(QDir::fromNativeSeparators(m_sdkPath) + "/sdk");
    if (sdkDir.exists()) {
        QStringList versions = sdkDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::Reversed);
        for (const QString &v : versions) {
            m_sdkVersionCombo->addItem(v);
        }
    }
    int idx = m_sdkVersionCombo->findText(m_sdkVersion);
    if (idx >= 0) m_sdkVersionCombo->setCurrentIndex(idx);
    pathLayout->addWidget(m_sdkVersionCombo, 1, 1);
    mainLayout->addWidget(pathGroup);

    QGroupBox *nameGroup = new QGroupBox(zh ? "工程名称" : "Project Name");
    QHBoxLayout *nameLayout = new QHBoxLayout(nameGroup);
    m_projectNameEdit = new QLineEdit(m_projectName);
    m_projectNameEdit->setMinimumHeight(28);
    nameLayout->addWidget(m_projectNameEdit);
    mainLayout->addWidget(nameGroup);

    QSplitter *splitter = new QSplitter(Qt::Vertical);

    QWidget *topWidget = new QWidget;
    QHBoxLayout *topLayout = new QHBoxLayout(topWidget);
    topLayout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *sourceGroup = new QGroupBox(zh ? "源文件" : "Source Files");
    QVBoxLayout *sourceLayout = new QVBoxLayout(sourceGroup);
    m_sourceList = new QListWidget;
    m_sourceList->addItems(m_sourceFiles);
    sourceLayout->addWidget(m_sourceList);
    QHBoxLayout *sourceBtnLayout = new QHBoxLayout;
    QPushButton *btnAdd = new QPushButton(zh ? "添加 .c/.cpp" : "Add .c/.cpp");
    QPushButton *btnRemove = new QPushButton(zh ? "删除选中" : "Remove Selected");
    btnAdd->setStyleSheet("QPushButton { background-color: #0078d4; color: white; font-weight: bold; padding: 4px 12px; border-radius: 3px; } QPushButton:hover { background-color: #1a8cd8; }");
    btnRemove->setStyleSheet("QPushButton { background-color: #ca5010; color: white; font-weight: bold; padding: 4px 12px; border-radius: 3px; } QPushButton:hover { background-color: #d45d1a; }");
    sourceBtnLayout->addWidget(btnAdd);
    sourceBtnLayout->addWidget(btnRemove);
    sourceBtnLayout->addStretch();
    sourceLayout->addLayout(sourceBtnLayout);
    connect(btnAdd, &QPushButton::clicked, this, &ProjectConfigDialog::onAddSource);
    connect(btnRemove, &QPushButton::clicked, this, &ProjectConfigDialog::onRemoveSource);

    QGroupBox *libGroup = new QGroupBox(zh ? "链接库" : "Link Libraries");
    QGridLayout *libLayout = new QGridLayout(libGroup);

    struct LibInfo { QString name; QString label; int platform; };
    // platform flags: 1=RP2040, 2=RP2350, 4=ARM, 8=RISC-V
    QList<LibInfo> allLibs = {
        {"pico_stdlib", "pico_stdlib (基础库)", 1|2|4|8},
        {"hardware_uart", "hardware_uart (串口)", 1|2|4|8},
        {"hardware_i2c", "hardware_i2c (I2C)", 1|2|4|8},
        {"hardware_spi", "hardware_spi (SPI)", 1|2|4|8},
        {"hardware_pwm", "hardware_pwm (PWM)", 1|2|4|8},
        {"hardware_adc", "hardware_adc (ADC)", 1|2|4|8},
        {"hardware_timer", "hardware_timer (定时器)", 1|2|4|8},
        {"hardware_watchdog", "hardware_watchdog (看门狗)", 1|2|4|8},
        {"hardware_flash", "hardware_flash (Flash)", 1|2|4|8},
        {"hardware_sync", "hardware_sync (同步)", 1|2|4|8},
        {"hardware_dma", "hardware_dma (DMA)", 1|2|4|8},
        {"hardware_pio", "hardware_pio (PIO)", 1|2|4|8},
        {"hardware_clocks", "hardware_clocks (时钟)", 1|2|4|8},
        {"hardware_irq", "hardware_irq (中断)", 1|2|4|8},
        {"hardware_gpio", "hardware_gpio (GPIO)", 1|2|4|8},
        {"hardware_rtc", "hardware_rtc (RTC)", 1|2|4|8},
        {"pico_multicore", "pico_multicore (多核)", 1|2|4|8},
        // RP2350-only libraries
        {"hardware_powman", "hardware_powman (电源管理) [RP2350]", 2|4|8},
        {"hardware_sha256", "hardware_sha256 (SHA256) [RP2350]", 2|4|8},
        {"hardware_dcp", "hardware_dcp (数据拷贝) [RP2350]", 2|4|8},
        {"hardware_rcp", "hardware_rcp (协处理器) [RP2350]", 2|8},
        {"hardware_riscv_platform_timer", "hardware_riscv_platform_timer [RP2350]", 2|8},
        {"pico_sha256", "pico_sha256 (SHA256库) [RP2350]", 2|4|8},
        {"pico_aon_timer", "pico_aon_timer (常开定时器) [RP2350]", 2|4|8},
        {"pico_atomic", "pico_atomic (原子操作) [RP2350]", 2|4|8},
        {"pico_rand", "pico_rand (随机数) [RP2350]", 2|4|8},
        // RISC-V only
        {"hardware_riscv", "hardware_riscv (RISC-V硬件) [RISC-V]", 2|8},
        {"hardware_hazard3", "hardware_hazard3 (Hazard3内核) [RISC-V]", 2|8},
        // ARM only
        {"cmsis", "cmsis (CMSIS) [ARM]", 2|4},
        // Network libraries
        {"pico_cyw43_arch_none", "pico_cyw43_arch (WiFi)", 1|2|4|8},
        {"pico_btstack_ble", "pico_btstack_ble (BLE)", 1|2|4|8},
        {"pico_lwip", "pico_lwip (LWIP网络)", 1|2|4|8},
    };

    // Determine current platform flags
    int currentFlags = 0;
    if (m_platform == "rp2040") currentFlags = 1;
    else if (m_platform == "rp2350-arm-s") currentFlags = 2|4;
    else if (m_platform == "rp2350-riscv") currentFlags = 2|8;

    // Filter libraries for this platform
    QList<LibInfo> libs;
    for (const auto &lib : allLibs) {
        if (lib.platform & currentFlags) {
            libs.append(lib);
        }
    }

    for (int i = 0; i < libs.size(); ++i) {
        QCheckBox *cb = new QCheckBox(libs[i].label);
        cb->setChecked(m_libraries.contains(libs[i].name));
        // Color-code platform-specific libraries
        if (libs[i].name.contains("powman") || libs[i].name.contains("sha256") ||
            libs[i].name.contains("dcp") || libs[i].name.contains("rcp") ||
            libs[i].name.contains("aon_timer") || libs[i].name.contains("atomic") ||
            libs[i].name.contains("pico_rand") ||
            libs[i].name.contains("riscv_platform_timer") || libs[i].name.contains("pico_sha256")) {
            cb->setStyleSheet("QCheckBox { color: #8B4513; font-weight: bold; font-size: 13px; } QCheckBox::indicator:checked { background-color: #0078d4; border-color: #0078d4; }");
        } else if (libs[i].name.contains("hardware_riscv") || libs[i].name.contains("hazard3")) {
            cb->setStyleSheet("QCheckBox { color: #006400; font-weight: bold; font-size: 13px; } QCheckBox::indicator:checked { background-color: #0078d4; border-color: #0078d4; }");
        } else if (libs[i].name == "cmsis") {
            cb->setStyleSheet("QCheckBox { color: #4a0080; font-weight: bold; font-size: 13px; } QCheckBox::indicator:checked { background-color: #0078d4; border-color: #0078d4; }");
        } else {
            cb->setStyleSheet("QCheckBox { color: #0078d4; font-weight: bold; font-size: 13px; } QCheckBox::indicator:checked { background-color: #0078d4; border-color: #0078d4; }");
        }
        libLayout->addWidget(cb, i / 2, i % 2);
        m_libCheckBoxes.append(cb);
        m_libMap[libs[i].name] = cb;
    }

    topLayout->addWidget(sourceGroup, 1);
    topLayout->addWidget(libGroup, 1);

    splitter->addWidget(topWidget);

    QWidget *bottomWidget = new QWidget;
    QVBoxLayout *bottomLayout = new QVBoxLayout(bottomWidget);
    bottomLayout->setContentsMargins(0, 0, 0, 0);

    QGroupBox *featureGroup = new QGroupBox(zh ? "功能开关" : "Feature Toggles");
    QHBoxLayout *featureLayout = new QHBoxLayout(featureGroup);
    m_stdioUsbCheck = new QCheckBox(zh ? "USB 串口输出" : "USB stdio");
    m_stdioUartCheck = new QCheckBox(zh ? "UART 串口输出" : "UART stdio");
    m_extraOutputsCheck = new QCheckBox(zh ? "生成额外输出文件 (.map/.bin/.hex)" : "Extra outputs (.map/.bin/.hex)");
    m_stdioUsbCheck->setChecked(m_stdioUsb);
    m_stdioUartCheck->setChecked(m_stdioUart);
    m_extraOutputsCheck->setChecked(m_extraOutputs);
    featureLayout->addWidget(m_stdioUsbCheck);
    featureLayout->addWidget(m_stdioUartCheck);
    featureLayout->addWidget(m_extraOutputsCheck);
    featureLayout->addStretch();

    QGroupBox *tcGroup = new QGroupBox(zh ? "工具链" : "Toolchain");
    QHBoxLayout *tcLayout = new QHBoxLayout(tcGroup);
    m_toolchainCombo = new QComboBox;
    m_toolchainCombo->setMinimumWidth(300);
    m_toolchainCombo->setStyleSheet("QComboBox { font-size: 13px; } QComboBox QAbstractItemView { font-size: 13px; }");

    if (m_platform == "rp2040") {
        m_toolchainCombo->addItem("pico_arm_cortex_m0plus_gcc (ARM Cortex-M0+ GCC)");
        m_toolchainCombo->addItem("pico_arm_cortex_m0plus_clang (ARM Cortex-M0+ Clang)");
    } else if (m_platform == "rp2350-arm-s") {
        m_toolchainCombo->addItem("pico_arm_cortex_m33_gcc (ARM Cortex-M33 GCC)");
        m_toolchainCombo->addItem("pico_arm_cortex_m33_clang (ARM Cortex-M33 Clang)");
        m_toolchainCombo->addItem("pico_arm_cortex_m23_gcc (ARM Cortex-M23 GCC)");
        m_toolchainCombo->addItem("pico_arm_clang_arm (ARM Clang)");
    } else if (m_platform == "rp2350-riscv") {
        m_toolchainCombo->addItem("pico_riscv_gcc_zcb_zcmp (RISC-V Zcb+Zcmp GCC)");
        m_toolchainCombo->addItem("pico_riscv_gcc (RISC-V GCC)");
    }

    if (!m_toolchain.isEmpty()) {
        for (int i = 0; i < m_toolchainCombo->count(); ++i) {
            if (m_toolchainCombo->itemText(i).startsWith(m_toolchain)) {
                m_toolchainCombo->setCurrentIndex(i);
                break;
            }
        }
    }

    tcLayout->addWidget(new QLabel(zh ? "编译器类型:" : "Compiler Type:"));
    tcLayout->addWidget(m_toolchainCombo, 1);
    tcLayout->addStretch();
    tcGroup->setStyleSheet("QGroupBox { color: #1a5276; font-weight: bold; font-size: 13px; border: 1px solid #b0b0b0; border-radius: 6px; margin-top: 14px; padding-top: 10px; background-color: #f5f5f5; } QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; background-color: #f5f5f5; }");

    QGroupBox *stdGroup = new QGroupBox(zh ? "语言标准" : "Language Standard");
    QHBoxLayout *stdLayout = new QHBoxLayout(stdGroup);
    stdLayout->addWidget(new QLabel(zh ? "C 标准:" : "C Standard:"));
    m_cStandardCombo = new QComboBox;
    m_cStandardCombo->addItems({"90", "99", "11", "17", "23"});
    m_cStandardCombo->setCurrentText(m_cStandard);
    stdLayout->addWidget(m_cStandardCombo);
    stdLayout->addSpacing(20);
    stdLayout->addWidget(new QLabel(zh ? "C++ 标准:" : "C++ Standard:"));
    m_cxxStandardCombo = new QComboBox;
    m_cxxStandardCombo->addItems({"98", "11", "14", "17", "20", "23"});
    m_cxxStandardCombo->setCurrentText(m_cxxStandard);
    stdLayout->addWidget(m_cxxStandardCombo);
    stdLayout->addStretch();

    bottomLayout->addWidget(featureGroup);
    bottomLayout->addWidget(tcGroup);
    bottomLayout->addWidget(stdGroup);

    splitter->addWidget(bottomWidget);
    splitter->setStretchFactor(0, 4);
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter, 1);

    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addStretch();
    QPushButton *btnSave = new QPushButton(zh ? "保存" : "Save");
    QPushButton *btnCancel = new QPushButton(zh ? "取消" : "Cancel");
    btnSave->setMinimumSize(100, 32);
    btnCancel->setMinimumSize(100, 32);
    btnSave->setStyleSheet("QPushButton { background-color: #107c10; color: white; font-weight: bold; font-size: 13px; border-radius: 4px; padding: 6px 16px; } QPushButton:hover { background-color: #1a8c1a; }");
    btnCancel->setStyleSheet("QPushButton { background-color: #888; color: white; font-size: 13px; border-radius: 4px; padding: 6px 16px; } QPushButton:hover { background-color: #999; }");
    btnLayout->addWidget(btnSave);
    btnLayout->addWidget(btnCancel);
    mainLayout->addLayout(btnLayout);

    connect(btnSave, &QPushButton::clicked, this, &ProjectConfigDialog::onSave);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

static QStringList scanSourceFilesInDir(const QString &dirPath)
{
    QStringList result;
    QDir dir(dirPath);
    QStringList filters = {"*.c", "*.cpp", "*.S", "*.s"};
    for (const QString &f : filters) {
        result.append(dir.entryList({f}, QDir::Files));
    }
    return result;
}

void ProjectConfigDialog::parseCMakeLists()
{
    QFile file(m_cmakePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QString content = QString::fromUtf8(file.readAll());
    file.close();
    QString srcDir = QFileInfo(m_cmakePath).absolutePath();

    // Parse PicoBuild toolchain comment
    QRegularExpression tcRe(R"(#\s*PicoBuild-Toolchain:\s*(\S+))");
    QRegularExpressionMatch tcm = tcRe.match(content);
    if (tcm.hasMatch())
        m_toolchain = tcm.captured(1);

    // Parse platform from PICO_PLATFORM or PICO_BOARD
    if (m_platform.isEmpty()) {
        QRegularExpression boardRe2("PICO_BOARD[:\"\\s=]+([a-zA-Z0-9_-]+)");
        QRegularExpressionMatch bm = boardRe2.match(content);
        if (bm.hasMatch() && bm.captured(1).contains("pico2"))
            m_platform = "rp2350-arm-s";
        QRegularExpression platRe2("PICO_PLATFORM[:\"\\s=]+([a-zA-Z0-9_-]+)");
        QRegularExpressionMatch pm2 = platRe2.match(content);
        if (pm2.hasMatch())
            m_platform = pm2.captured(1);
    }

    QRegularExpression projectRe(R"(project\s*\(\s*(\S+))");
    QRegularExpressionMatch pm = projectRe.match(content);
    if (pm.hasMatch())
        m_projectName = pm.captured(1);

    QRegularExpression sourcesRe(R"(add_executable\s*\(\s*(\S+)([\s\S]*?)\))");
    QRegularExpressionMatch sm = sourcesRe.match(content);
    if (sm.hasMatch()) {
        QString srcBlock = sm.captured(2);
        QRegularExpression srcRe(R"((\S+\.c\S*|\S+\.cpp\S*|\S+\.s\S*|\S+\.S\S*))");
        QRegularExpressionMatchIterator it = srcRe.globalMatch(srcBlock);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            QString src = m.captured(1);
            if (src.endsWith(".c") || src.endsWith(".cpp") || src.endsWith(".S") || src.endsWith(".s"))
                m_sourceFiles.append(src);
        }
    }

    if (m_sourceFiles.isEmpty()) {
        m_sourceFiles = scanSourceFilesInDir(srcDir);
    }

    QRegularExpression libsRe(R"(target_link_libraries\s*\(\s*(\S+)([\s\S]*?)\))");
    QRegularExpressionMatch lm = libsRe.match(content);
    if (lm.hasMatch()) {
        QString libBlock = lm.captured(2);
        QRegularExpression libRe(R"((\S+))");
        QRegularExpressionMatchIterator it = libRe.globalMatch(libBlock);
        while (it.hasNext()) {
            QRegularExpressionMatch m = it.next();
            QString lib = m.captured(1);
            if (!lib.contains("::") && !lib.startsWith("$") && !lib.startsWith("${"))
                m_libraries.append(lib);
        }
    }

    QRegularExpression usbRe(R"(pico_enable_stdio_usb\s*\(\s*\S+\s+(\d+)\s*\))");
    QRegularExpressionMatch um = usbRe.match(content);
    if (um.hasMatch())
        m_stdioUsb = (um.captured(1) == "1");

    QRegularExpression uartRe(R"(pico_enable_stdio_uart\s*\(\s*\S+\s+(\d+)\s*\))");
    QRegularExpressionMatch urm = uartRe.match(content);
    if (urm.hasMatch())
        m_stdioUart = (urm.captured(1) == "1");

    QRegularExpression extraRe(R"(pico_add_extra_outputs\s*\(\s*\S+\s*\))");
    m_extraOutputs = extraRe.match(content).hasMatch();

    QRegularExpression cStdRe(R"(CMAKE_C_STANDARD\s+(\d+))");
    QRegularExpressionMatch csm = cStdRe.match(content);
    if (csm.hasMatch())
        m_cStandard = csm.captured(1);

    QRegularExpression cxxStdRe(R"(CMAKE_CXX_STANDARD\s+(\d+))");
    QRegularExpressionMatch cxxsm = cxxStdRe.match(content);
    if (cxxsm.hasMatch())
        m_cxxStandard = cxxsm.captured(1);
}

QString ProjectConfigDialog::generateCMakeLists() const
{
    QString projectName = m_projectNameEdit->text().trimmed();
    if (projectName.isEmpty())
        projectName = "pico_project";

    QStringList sources;
    for (int i = 0; i < m_sourceList->count(); ++i)
        sources.append(m_sourceList->item(i)->text());

    if (sources.isEmpty()) {
        QString srcDir = QFileInfo(m_cmakePath).absolutePath();
        sources = scanSourceFilesInDir(srcDir);
    }

    QStringList libs;
    for (auto it = m_libMap.begin(); it != m_libMap.end(); ++it) {
        if (it.value()->isChecked())
            libs.append(it.key());
    }

    QString result;
    result += "# Generated by PicoBuild\n";
    result += "cmake_minimum_required(VERSION 3.13)\n\n";
    result += "set(CMAKE_C_STANDARD " + m_cStandardCombo->currentText() + ")\n";
    result += "set(CMAKE_CXX_STANDARD " + m_cxxStandardCombo->currentText() + ")\n\n";

    result += "set(PICO_BOARD pico CACHE STRING \"Board type\")\n\n";

    result += "# Pull in SDK (must be before project)\n";
    result += "include(pico_sdk_import.cmake)\n\n";
    result += "project(" + projectName + " C CXX ASM)\n\n";
    result += "pico_sdk_init()\n\n";
    result += "# PicoBuild-Toolchain: " + m_toolchain + "\n\n";
    result += "add_executable(" + projectName + "\n";
    for (const QString &s : sources)
        result += "    " + s + "\n";
    result += ")\n\n";
    result += "target_include_directories(" + projectName + " PRIVATE ${CMAKE_CURRENT_LIST_DIR} ${CMAKE_CURRENT_BINARY_DIR})\n\n";

    QStringList pioFiles;
    if (libs.contains("hardware_pio")) {
        QString pioDir = QFileInfo(m_cmakePath).absolutePath();
        QDirIterator pioIt(pioDir, QStringList() << "*.pio", QDir::Files);
        while (pioIt.hasNext()) {
            pioFiles.append(QFileInfo(pioIt.next()).fileName());
        }
    }
    for (const QString &pf : pioFiles) {
        result += "pico_generate_pio_header(" + projectName + " ${CMAKE_CURRENT_LIST_DIR}/" + pf + ")\n";
    }
    if (!pioFiles.isEmpty()) {
        result += "\n";
    }

    result += "target_link_libraries(" + projectName + "\n";
    for (const QString &lib : libs) {
        result += "    " + lib + "\n";
    }
    result += ")\n\n";

    if (m_stdioUsbCheck->isChecked())
        result += "pico_enable_stdio_usb(" + projectName + " 1)\n";
    if (m_stdioUartCheck->isChecked())
        result += "pico_enable_stdio_uart(" + projectName + " 1)\n";

    if (m_extraOutputsCheck->isChecked())
        result += "\npico_add_extra_outputs(" + projectName + ")\n";

    QString projectDir = QFileInfo(m_cmakePath).absolutePath();
    QString vscodeDir = projectDir + "/.vscode";
    QDir().mkpath(vscodeDir);

    QString vsSettings = "{\n";
    vsSettings += "    \"C_Cpp.default.compileCommands\": \"${workspaceFolder}/.vscode/compile_commands.json\",\n";
    vsSettings += "    \"C_Cpp.default.intelliSenseMode\": \"gcc-arm64\",\n";
    vsSettings += "    \"C_Cpp.default.cStandard\": \"c" + m_cStandardCombo->currentText() + "\",\n";
    vsSettings += "    \"C_Cpp.default.cppStandard\": \"c++" + m_cxxStandardCombo->currentText() + "\"\n";
    vsSettings += "}\n";

    QFile vsFile(vscodeDir + "/settings.json");
    if (vsFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        vsFile.write(vsSettings.toUtf8());
        vsFile.close();
    }

    return result;
}

static QMap<QString, QStringList> s_libHeaders()
{
    static QMap<QString, QStringList> map;
    if (map.isEmpty()) {
        map["pico_stdlib"]        = {"pico/stdlib.h"};
        map["hardware_spi"]       = {"hardware/spi.h"};
        map["hardware_i2c"]       = {"hardware/i2c.h"};
        map["hardware_uart"]      = {"hardware/uart.h"};
        map["hardware_pwm"]       = {"hardware/pwm.h"};
        map["hardware_adc"]       = {"hardware/adc.h"};
        map["hardware_timer"]     = {"hardware/timer.h", "pico/time.h"};
        map["hardware_watchdog"]  = {"hardware/watchdog.h"};
        map["hardware_flash"]     = {"hardware/flash.h"};
        map["hardware_sync"]      = {"hardware/sync.h", "pico/critical_section.h"};
        map["hardware_dma"]       = {"hardware/dma.h"};
        map["hardware_pio"]       = {"hardware/pio.h"};
        map["hardware_rtc"]       = {"hardware/rtc.h", "pico/sleep.h"};
        map["hardware_clocks"]    = {"hardware/clocks.h"};
        map["hardware_irq"]       = {"hardware/irq.h"};
        map["hardware_gpio"]      = {"hardware/gpio.h"};
        map["pico_multicore"]     = {"pico/multicore.h"};
        map["hardware_powman"]    = {"hardware/powman.h"};
        map["hardware_sha256"]    = {"hardware/sha256.h"};
        map["hardware_dcp"]       = {"hardware/dcp.h"};
        map["hardware_rcp"]       = {"hardware/rcp.h"};
        map["hardware_riscv_platform_timer"] = {"hardware/riscv_platform_timer.h"};
        map["pico_sha256"]        = {"pico/sha256.h"};
        map["pico_aon_timer"]     = {"pico/aon_timer.h"};
        map["pico_atomic"]        = {"pico/atomic.h"};
        map["pico_rand"]          = {"pico/rand.h"};
        map["hardware_riscv"]     = {"hardware/riscv.h"};
        map["hardware_hazard3"]   = {"hardware/hazard3.h"};
        map["cmsis"]              = {"cmsis/arm_math.h", "cmsis/core_cm33.h"};
        map["pico_cyw43_arch_none"] = {"pico/cyw43_arch.h"};
        map["pico_btstack_ble"]   = {"btstack.h", "pico/btstack_ble.h"};
        map["pico_lwip"]          = {"lwip/init.h", "lwip/opt.h", "lwip/tcp.h", "lwip/udp.h"};
    }
    return map;
}

static void syncSourceIncludes(const QString &srcDir, const QString &srcFile,
                                const QMap<QString, QCheckBox *> &libMap)
{
    QString filePath = srcDir + "/" + srcFile;
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    QString content = QString::fromUtf8(f.readAll());
    f.close();

    QMap<QString, QStringList> headers = s_libHeaders();
    QMap<QString, QCheckBox *> libMapRef = libMap;

    QStringList lines = content.split('\n');
    int lastIncludeIdx = -1;

    for (int i = 0; i < lines.size(); ++i) {
        if (lines[i].trimmed().startsWith("#include"))
            lastIncludeIdx = i;
    }

    for (auto it = libMapRef.begin(); it != libMapRef.end(); ++it) {
        if (!headers.contains(it.key()))
            continue;

        QStringList hdrs = headers[it.key()];
        for (const QString &hdr : hdrs) {
            QRegularExpression includeRe("#include\\s*[\"<]" + QRegularExpression::escape(hdr) + "[\">]");
            bool hasInclude = includeRe.match(content).hasMatch();

            if (it.value()->isChecked() && !hasInclude) {
                QString newLine = "#include \"" + hdr + "\"";
                lines.insert(lastIncludeIdx + 1, newLine);
                lastIncludeIdx++;
            } else if (!it.value()->isChecked() && hasInclude) {
                for (int i = lines.size() - 1; i >= 0; --i) {
                    if (includeRe.match(lines[i]).hasMatch()) {
                        lines.removeAt(i);
                        if (i <= lastIncludeIdx)
                            lastIncludeIdx--;
                    }
                }
            }
        }
    }

    QFile out(filePath);
    if (out.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        out.write(lines.join('\n').toUtf8());
        out.close();
    }
}

void ProjectConfigDialog::onPreview()
{
}

void ProjectConfigDialog::onAddSource()
{
    QStringList files;
    QDir srcDir(QFileInfo(m_cmakePath).absolutePath());
    QStringList filters = {"*.c", "*.cpp", "*.S", "*.s"};
    for (const QString &f : filters) {
        QStringList found = srcDir.entryList({f}, QDir::Files);
        for (const QString &fn : found) {
            if (!m_sourceList->findItems(fn, Qt::MatchExactly).isEmpty())
                continue;
            files.append(fn);
        }
    }

    if (files.isEmpty()) {
        QMessageBox::information(this, LanguageManager::isChinese() ? "提示" : "Info",
                                 LanguageManager::isChinese() ? "未找到可添加的源文件。\n请在工程目录下放置 .c/.cpp/.S 文件。"
                                                              : "No source files found to add.\nPlease place .c/.cpp/.S files in the project directory.");
        return;
    }

    QString selected = files.first();
    for (const QString &f : files) {
        if (m_sourceList->findItems(f, Qt::MatchExactly).isEmpty()) {
            selected = f;
            break;
        }
    }

    m_sourceList->addItem(selected);
}

void ProjectConfigDialog::onRemoveSource()
{
    QListWidgetItem *item = m_sourceList->currentItem();
    if (item) {
        delete m_sourceList->takeItem(m_sourceList->row(item));
    }
}

void ProjectConfigDialog::onSave()
{
    bool zh = LanguageManager::isChinese();
    if (m_sourceList->count() == 0) {
        QMessageBox::warning(this, zh ? "警告" : "Warning",
                             zh ? "未配置任何源文件！\n\n"
                                 "请点击【添加 .c/.cpp】按钮从工程目录中选择源文件，\n"
                                 "或在工程目录下放置 .c/.cpp/.S 文件后重新打开此窗口。"
                               : "No source files configured!\n\n"
                                 "Click [Add .c/.cpp] to select source files from the project directory,\n"
                                 "or place .c/.cpp/.S files in the project directory and reopen this window.");
        return;
    }

    // Extract toolchain name from the combo text (before the first space/parenthesis)
    QString tcText = m_toolchainCombo->currentText();
    int spaceIdx = tcText.indexOf(' ');
    m_toolchain = (spaceIdx > 0) ? tcText.left(spaceIdx) : tcText;

    QString content = generateCMakeLists();
    QFile file(m_cmakePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, zh ? "错误" : "Error", zh ? "无法写入 CMakeLists.txt:\n" : "Cannot write CMakeLists.txt:\n" + file.errorString());
        return;
    }
    file.write(content.toUtf8());
    file.close();

    m_sdkPath = m_sdkPathEdit->text().trimmed();
    m_sdkVersion = m_sdkVersionCombo->currentText();

    QString srcDir = QFileInfo(m_cmakePath).absolutePath();
    for (int i = 0; i < m_sourceList->count(); ++i) {
        QString srcFile = m_sourceList->item(i)->text();
        if (srcFile.endsWith(".c") || srcFile.endsWith(".cpp"))
            syncSourceIncludes(srcDir, srcFile, m_libMap);
    }

    accept();
}