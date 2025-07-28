#include "TaskRouter.hpp"
#include <QtHttpServer/QHttpServerResponse>
#include <QtHttpServer/QHttpServerRequest>

TaskRouter::TaskRouter(std::shared_ptr<ITaskService> service)
    : m_service(std::move(service)) {}

void TaskRouter::registerRoutes(QHttpServer &server) {
    server.route("/ping", [] {
        return QHttpServerResponse("text/plain", "pong");
    });

    // todo: task routes will be added here
}
