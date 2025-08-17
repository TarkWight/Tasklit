#include <QCoreApplication>
#include <QtHttpServer/QHttpServer>
#include <QtHttpServer/QHttpServerResponse>
#include <QTcpServer>
#include <QHostAddress>
#include <QDebug>

#include "Logger.hpp"
#include "SQLiteStorageImpl.hpp"
#include "TaskServiceImpl.hpp"
#include "TaskRouter.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    initLogging("tasklit.log");

    QLoggingCategory::setFilterRules(
        "tasklit.*=true\n"
        "qt.network.ssl.warning=false\n"
        );

    // ──────────────────────────────
    // 1. Настраиваем хранилище
    // ──────────────────────────────
    auto storage = std::make_shared<SQLiteStorage>("tasks.db");

    // ──────────────────────────────
    // 2. Создаём сервис задач
    // ──────────────────────────────
    auto service = std::make_shared<TaskServiceImpl>(storage);

    // ──────────────────────────────
    // 3. Создаём HTTP-сервер
    // ──────────────────────────────
    QHttpServer server;

    // ──────────────────────────────
    // 4. Прокидываем маршруты
    // ──────────────────────────────
    TaskRouter router(service);
    router.registerRoutes(server);

    // ──────────────────────────────
    // 5. Привязка и запуск
    // ──────────────────────────────
    auto tcp = new QTcpServer(&app);
    if (!tcp->listen(QHostAddress::Any, 8080) || !server.bind(tcp)) {
        qWarning() << "Server failed to start";
        return 1;
    }

    qInfo() << "Server running on port" << tcp->serverPort();
    return app.exec();
}
