// Default empty project template
#include <bb/cascades/Application>
#include <bb/cascades/QmlDocument>
#include <bb/cascades/AbstractPane>

#include <QLocale>
#include <QTranslator>
#include <Qt/qdeclarativedebug.h>
#include "GeneticCloner.h"

using namespace bb::cascades;

Q_DECL_EXPORT int main(int argc, char **argv)
{
    Application app(argc, argv);

    // localization support
    QTranslator translator;
    QString locale_string = QLocale().name();
    QString filename = QString("GeneticCloner_%1").arg(locale_string);
    if (translator.load(filename, "app/native/qm"))
    {
        app.installTranslator(&translator);
    }

    GeneticCloner* clone = new GeneticCloner(&app);
    clone->init(&app);

    return app.exec();
}
