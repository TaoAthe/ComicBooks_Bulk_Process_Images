#include "serviceapp.h"

#include "backendcontroller.h"
#include "apiserver.h"

#include <QDebug>

ServiceApp::ServiceApp(QObject *parent)
    : QObject(parent),
      m_backend(new BackendController(this)),
      m_apiServer(new ApiServer(m_backend, this)) {}

ServiceApp::~ServiceApp() = default;

bool ServiceApp::start() {
    constexpr quint16 kDefaultPort = 5500;
    if (!m_apiServer->start(kDefaultPort)) {
        qCritical() << "Unable to launch API server";
        return false;
    }
    return true;
}