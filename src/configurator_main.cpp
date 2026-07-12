#include "configurator.hpp"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("InstallerMaker"));

    Configurator dlg;
    dlg.show();

    return app.exec();
}
