#include "mainwindow.h"
#include "translator.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>

#ifdef Q_OS_WIN
#include <wincon.h>
#include <windows.h>
#endif

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    SetConsoleCtrlHandler(NULL, TRUE);
#endif
    // We set application info up.
    // It is needed to be able to store settings easily.
    QCoreApplication::setOrganizationName("Openwall");
    QCoreApplication::setOrganizationDomain("openwall.com");
    QCoreApplication::setApplicationName("ny");

    QApplication app(argc, argv);

    // Setting the application version which is defined in ny.pro
    app.setApplicationVersion(APP_VERSION);

    QSettings settings(
        QDir(QDir::home().filePath(".on")).filePath("ny.conf"),
        QSettings::IniFormat);
    QString     settingLanguage = settings.value("Language").toString();
    Translator &translator      = Translator::getInstance();

    // If no language is saved : default behavior is to use system language
    if(settingLanguage.isEmpty())
    {
        QString systemLanguage = QLocale::languageToString(QLocale().language());
        translator.translateApplication(&app, systemLanguage);
        settings.setValue("Language", translator.getCurrentLanguage().toLower());
    }
    else
    {
        // Use the language specified in the settings
        translator.translateApplication(&app, settingLanguage);
    }

    MainWindow window(settings);
    window.show();

    return app.exec();
}
