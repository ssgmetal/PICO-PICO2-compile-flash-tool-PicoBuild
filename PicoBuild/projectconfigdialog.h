#ifndef PROJECTCONFIGDIALOG_H
#define PROJECTCONFIGDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QListWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QMap>
#include <QList>

class ProjectConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ProjectConfigDialog(const QString &cmakePath, const QString &sdkPath,
                                  const QString &platform, const QString &toolchain,
                                  const QString &sdkVersion,
                                  QWidget *parent = nullptr);
    QString sdkPath() const { return m_sdkPath; }
    QString toolchain() const { return m_toolchain; }
    QString sdkVersion() const { return m_sdkVersion; }

private slots:
    void onAddSource();
    void onRemoveSource();
    void onSave();
    void onPreview();

private:
    void parseCMakeLists();
    QString generateCMakeLists() const;
    void setupUI();
    void addLibraryCheckBox(const QString &name, const QString &label, QList<QCheckBox *> &list);

    QString m_cmakePath;
    QString m_sdkPath;
    QString m_platform;
    QString m_toolchain;
    QString m_sdkVersion;
    QString m_projectName;
    QStringList m_sourceFiles;
    QStringList m_libraries;
    QString m_cStandard;
    QString m_cxxStandard;
    bool m_stdioUsb;
    bool m_stdioUart;
    bool m_extraOutputs;

    QLineEdit *m_projectNameEdit;
    QLineEdit *m_sdkPathEdit;
    QListWidget *m_sourceList;
    QList<QCheckBox *> m_libCheckBoxes;
    QMap<QString, QCheckBox *> m_libMap;
    QCheckBox *m_stdioUsbCheck;
    QCheckBox *m_stdioUartCheck;
    QCheckBox *m_extraOutputsCheck;
    QComboBox *m_cStandardCombo;
    QComboBox *m_cxxStandardCombo;
    QComboBox *m_toolchainCombo;
    QComboBox *m_sdkVersionCombo;
};

#endif // PROJECTCONFIGDIALOG_H