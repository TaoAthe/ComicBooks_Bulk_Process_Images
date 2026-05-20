#ifndef SERVICEAPP_H
#define SERVICEAPP_H

#include <QObject>

class BackendController;
class ApiServer;

class ServiceApp : public QObject {
    Q_OBJECT

public:
    explicit ServiceApp(QObject *parent = nullptr);
    ~ServiceApp();

    bool start();

private:
    BackendController *m_backend;
    ApiServer *m_apiServer;
};

#endif // SERVICEAPP_H