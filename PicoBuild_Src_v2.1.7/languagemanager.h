#ifndef LANGUAGEMANAGER_H
#define LANGUAGEMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>

class LanguageManager : public QObject
{
    Q_OBJECT

public:
    enum Language { Chinese, English };

    static LanguageManager &instance();
    static QString tr(const QString &zhText);
    static QString tr(const QString &zhText, const QString &enText);

    Language currentLanguage() const;
    void setLanguage(Language lang);
    static bool isChinese();

signals:
    void languageChanged();

private:
    LanguageManager();
    void loadTranslations();

    Language m_currentLang;
    QMap<QString, QString> m_zhToEn;
};

#endif // LANGUAGEMANAGER_H