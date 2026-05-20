#include <QApplication>
#include "serviceapp.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    ServiceApp service;
    if (!service.start()) {
        return 1;
    }
    return app.exec();
}