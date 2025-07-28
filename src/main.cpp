#include <QCoreApplication>
#include <QtHttpServer/QHttpServer>
#include <QtHttpServer/QHttpServerResponse>
#include <QTcpServer>
#include <QHostAddress>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    QHttpServer server;
    server.route("/ping", []() {
        return QHttpServerResponse("text/plain", "pong");
    });

    auto tcp = new QTcpServer(&app);
    if (!tcp->listen(QHostAddress::Any, 8080) || !server.bind(tcp)) {
        qWarning() << "Server failed to start";
        return 1;
    }
    qInfo() << "Server running on port" << tcp->serverPort();

    return app.exec();
}
