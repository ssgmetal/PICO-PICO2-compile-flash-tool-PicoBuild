#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QProcess>
#include <QDir>
#include <QMenu>
#include <QAction>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    QString sourceDir() const;
    QString sdkPath() const;
    void setSdkPath(const QString &path);

private slots:
    void onNewProject();
    void onOpenProject();
    void onConfigure();
    void onBuild();
    void onClean();
    void onFlash();
    void onProjectConfig();
    void onAbout();
    void onExit();
    void onProcessOutput();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);
    void onClearOutput();
    void updateLanguage();

private:
    enum CurrentOperation {
        OpNone,
        OpConfigure,
        OpBuild,
        OpClean,
        OpFlash
    };

    void appendOutput(const QString &text, const QColor &color = QColor("#d4d4d4"), bool bold = false, const QString &fontSize = "", bool replaceLast = false);
    void highlightOutput(const QString &title, const QStringList &items);
    void runCommand(const QString &program, const QStringList &arguments, const QString &workingDir = QString(), const QProcessEnvironment &env = QProcessEnvironment());
    void setButtonsEnabled(bool enabled);
    QString resolveToolPath(const QString &relativePath) const;
    QString findElfFile(const QString &buildDir) const;
    bool validateInputs();
    QString findLatestVersion(const QString &baseDir, const QString &pattern) const;
    void updateProgressFromText(const QString &text);
    void loadConfig();
    void saveConfig();
    void formatOutputLine(const QString &line, QColor &outColor, bool &outBold, QString &outFontSize);
    void displayMemoryInfo();
    QString autoDetectSdk();
    void scanSdkInternal();
    void openProject(const QString &sourceDir);
    void updateProjectDisplay();
    QString platformValue() const;

    Ui::MainWindow *ui;
    QProcess *m_process;
    QMenu *m_langMenu = nullptr;
    QAction *m_actChinese = nullptr;
    QAction *m_actEnglish = nullptr;
    bool m_configured;
    CurrentOperation m_currentOp;

    QString m_sourceDir;
    QString m_platform;
    QString m_toolchain;
    QString m_sdkPath;
    QString m_sdkBaseDir;
    QString m_cmakePath;
    QString m_ninjaPath;
    QString m_picoSdkPath;
    QString m_toolchainPath;
    QString m_riscvToolchainPath;
    QString m_pioasmPath;
    QString m_pythonExe;
    QString m_picotoolPath;
    bool m_hasProgressLine = false;
    QStringList m_memoryLines;
};

#endif // MAINWINDOW_H