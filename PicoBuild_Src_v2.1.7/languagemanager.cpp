#include "languagemanager.h"
#include <QApplication>

LanguageManager &LanguageManager::instance()
{
    static LanguageManager inst;
    return inst;
}

LanguageManager::LanguageManager()
    : m_currentLang(Chinese)
{
    loadTranslations();
}

void LanguageManager::loadTranslations()
{
    m_zhToEn.clear();

    m_zhToEn["PicoBuild - RP2040/RP2350 嵌入式开发工具"] = "PicoBuild - RP2040/RP2350 Embedded Dev Tool";
    m_zhToEn["PicoBuild"] = "PicoBuild";

    m_zhToEn["新建工程"] = "New Project";
    m_zhToEn["打开工程"] = "Open Project";
    m_zhToEn["工程配置"] = "Project Config";
    m_zhToEn["配置"] = "Configure";
    m_zhToEn["编译"] = "Build";
    m_zhToEn["清理"] = "Clean";
    m_zhToEn["下载"] = "Flash";
    m_zhToEn["关于"] = "About";
    m_zhToEn["退出"] = "Exit";

    m_zhToEn["工程名称"] = "Project Name";
    m_zhToEn["工程路径"] = "Project Path";
    m_zhToEn["SDK 路径"] = "SDK Path";
    m_zhToEn["SDK 版本"] = "SDK Version";
    m_zhToEn["源文件"] = "Source Files";
    m_zhToEn["添加源文件"] = "Add Source";
    m_zhToEn["删除源文件"] = "Remove Source";
    m_zhToEn["C 标准"] = "C Standard";
    m_zhToEn["C++ 标准"] = "C++ Standard";
    m_zhToEn["工具链"] = "Toolchain";
    m_zhToEn["输出选项"] = "Output Options";
    m_zhToEn["USB stdio"] = "USB stdio";
    m_zhToEn["UART stdio"] = "UART stdio";
    m_zhToEn["生成额外文件"] = "Extra Outputs";
    m_zhToEn["保存"] = "Save";
    m_zhToEn["取消"] = "Cancel";
    m_zhToEn["预览"] = "Preview";
    m_zhToEn["浏览"] = "Browse";
    m_zhToEn["浏览..."] = "Browse...";

    m_zhToEn["芯片配置"] = "Chip Config";
    m_zhToEn["硬件库"] = "Hardware Libraries";
    m_zhToEn["基础库"] = "Basic Libraries";
    m_zhToEn["无线电库"] = "Wireless Libraries";
    m_zhToEn["USB 库"] = "USB Libraries";
    m_zhToEn["文件系统库"] = "Filesystem Libraries";
    m_zhToEn["图形库"] = "Graphics Libraries";
    m_zhToEn["其他库"] = "Other Libraries";

    m_zhToEn["SDK 扫描"] = "SDK Scan";
    m_zhToEn["SDK 根目录"] = "SDK Root";
    m_zhToEn["SDK 不完整"] = "SDK Incomplete";
    m_zhToEn["部分工具未找到，请检查 SDK 目录"] = "Some tools not found, check SDK directory";
    m_zhToEn["未找到"] = "Not Found";
    m_zhToEn["未找到 .pico-sdk 目录"] = ".pico-sdk directory not found";

    m_zhToEn["CMake 配置"] = "CMake Configure";
    m_zhToEn["编译"] = "Build";
    m_zhToEn["清理"] = "Clean";
    m_zhToEn["烧录"] = "Flash";
    m_zhToEn["操作"] = "Operation";

    m_zhToEn["配置 成功"] = "Configure Succeeded";
    m_zhToEn["配置 失败"] = "Configure Failed";
    m_zhToEn["编译 成功"] = "Build Succeeded";
    m_zhToEn["编译 失败"] = "Build Failed";
    m_zhToEn["清理 成功"] = "Clean Succeeded";
    m_zhToEn["清理 失败"] = "Clean Failed";
    m_zhToEn["烧录 成功"] = "Flash Succeeded";
    m_zhToEn["烧录 失败"] = "Flash Failed";

    m_zhToEn["配置"] = "Configure";
    m_zhToEn["编译"] = "Build";
    m_zhToEn["清理"] = "Clean";
    m_zhToEn["烧录"] = "Flash";

    m_zhToEn["源码目录"] = "Source Dir";
    m_zhToEn["构建目录"] = "Build Dir";
    m_zhToEn["SDK 路径"] = "SDK Path";
    m_zhToEn["工具链"] = "Toolchain";
    m_zhToEn["编译器"] = "Compiler";
    m_zhToEn["执行"] = "Execute";

    m_zhToEn["CMake"] = "CMake";
    m_zhToEn["Ninja"] = "Ninja";
    m_zhToEn["Pico SDK"] = "Pico SDK";
    m_zhToEn["ARM工具链"] = "ARM Toolchain";
    m_zhToEn["RISC-V工具链"] = "RISC-V Toolchain";
    m_zhToEn["Pioasm"] = "Pioasm";
    m_zhToEn["Python3"] = "Python3";
    m_zhToEn["Picotool"] = "Picotool";

    m_zhToEn["picotool 未找到，已禁用在线下载 (需 git)"] = "picotool not found, disabled online download (needs git)";

    m_zhToEn["已生成 .vscode/compile_commands.json (VS Code 代码补全)"] = "Generated .vscode/compile_commands.json (VS Code IntelliSense)";

    m_zhToEn["工程配置已更新"] = "Project config updated";
    m_zhToEn["请选择保存位置！"] = "Please select save location!";
    m_zhToEn["请填写工程名称！"] = "Please enter project name!";
    m_zhToEn["请选择源文件！"] = "Please select source files!";
    m_zhToEn["输入错误"] = "Input Error";
    m_zhToEn["提示"] = "Info";
    m_zhToEn["错误"] = "Error";
    m_zhToEn["警告"] = "Warning";
    m_zhToEn["确认"] = "Confirm";

    m_zhToEn["清空输出"] = "Clear Output";
    m_zhToEn["全选"] = "Select All";
    m_zhToEn["复制"] = "Copy";

    m_zhToEn["未找到 CMakeLists.txt 文件！"] = "CMakeLists.txt not found!";
    m_zhToEn["请先选择工程目录！"] = "Please select project directory first!";
    m_zhToEn["请先配置工程！"] = "Please configure project first!";
    m_zhToEn["请先编译工程！"] = "Please build project first!";
    m_zhToEn["未找到 ELF 文件，请先编译！"] = "ELF file not found, please build first!";
    m_zhToEn["未找到 UF2 文件，请先编译！"] = "UF2 file not found, please build first!";
    m_zhToEn["未找到烧录工具 picotool！"] = "Picotool not found!";
    m_zhToEn["SDK 环境不完整，请在设置中配置 SDK 路径"] = "SDK incomplete, configure SDK path in settings";
    m_zhToEn["SDK 配置不完整，请先扫描 SDK"] = "SDK incomplete, please scan SDK first";
    m_zhToEn["未找到 CMake 可执行文件"] = "CMake executable not found";
    m_zhToEn["未找到 Ninja 可执行文件"] = "Ninja executable not found";

    m_zhToEn["新建工程需要选择 Pico SDK 目录"] = "New project requires Pico SDK directory";
    m_zhToEn["确定要退出吗？"] = "Are you sure you want to exit?";
    m_zhToEn["确定要清理工程吗？"] = "Are you sure you want to clean the project?";
    m_zhToEn["清理将删除整个 build 目录"] = "This will delete the entire build directory";
    m_zhToEn["选择工程目录"] = "Select Project Directory";
    m_zhToEn["选择 SDK 目录 (.pico-sdk)"] = "Select SDK Directory (.pico-sdk)";
    m_zhToEn["选择工程存放目录"] = "Select Project Save Location";
    m_zhToEn["选择源文件"] = "Select Source Files";
    m_zhToEn["C/C++ 源文件 (*.c *.cpp *.h *.S)"] = "C/C++ Source Files (*.c *.cpp *.h *.S)";

    m_zhToEn["选择"] = "Select";
    m_zhToEn["打开"] = "Open";
    m_zhToEn["是"] = "Yes";
    m_zhToEn["否"] = "No";

    m_zhToEn["Flash Memory:"] = "Flash Memory:";
    m_zhToEn["RAM:"] = "RAM:";
    m_zhToEn["已使用"] = "Used";
    m_zhToEn["共"] = "Total";

    m_zhToEn["语言 (&L)"] = "&Language";
    m_zhToEn["中文 (&Z)"] = "&Chinese";
    m_zhToEn["English (&E)"] = "&English";

    m_zhToEn["Raspberry Pi Pico / Pico2 Embedded Development Tool"] = "Raspberry Pi Pico / Pico2 Embedded Development Tool";
    m_zhToEn["Features:"] = "Features:";
    m_zhToEn["New Project / Open Project / CMake Config / Build / Flash\nProject Config / Library Sync / Auto Header Include"] = "New Project / Open Project / CMake Config / Build / Flash\nProject Config / Library Sync / Auto Header Include";
    m_zhToEn["Updated:"] = "Updated:";
    m_zhToEn["Chip Support:"] = "Chip Support:";
    m_zhToEn["Build System:"] = "Build System:";
    m_zhToEn["SDK:"] = "SDK:";
    m_zhToEn["Version:"] = "Version:";
}

QString LanguageManager::tr(const QString &zhText)
{
    return instance().m_zhToEn.value(zhText, zhText);
}

QString LanguageManager::tr(const QString &zhText, const QString &enText)
{
    Q_UNUSED(enText);
    if (instance().m_currentLang == English) {
        return instance().m_zhToEn.value(zhText, enText.isEmpty() ? zhText : enText);
    }
    return zhText;
}

LanguageManager::Language LanguageManager::currentLanguage() const
{
    return m_currentLang;
}

void LanguageManager::setLanguage(Language lang)
{
    if (m_currentLang != lang) {
        m_currentLang = lang;
        emit languageChanged();
    }
}

bool LanguageManager::isChinese()
{
    return instance().m_currentLang == Chinese;
}