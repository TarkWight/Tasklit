#include <QtHttpServer/QHttpServerResponse>
#include <QtHttpServer/QHttpServerRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "TaskRouter.hpp"
#include "json_utils.hpp"

TaskRouter::TaskRouter(std::shared_ptr<ITaskService> service)
    : m_service(std::move(service)) {}

void TaskRouter::registerRoutes(QHttpServer &server) {
    // ping
    server.route("/ping", [] {
        return QHttpServerResponse("text/plain", "pong");
    });

    // GET /tasks — список
    server.route("/tasks", [this] {
        QJsonArray arr;
        for (const auto &t : m_service->getAllTasks()) {
            arr.append(t.toJson());
        }
        return makeJsonArray(arr);
    });

    // GET /tasks/<int> — одна задача
    server.route("/tasks/<int>", [this](int id) {
        auto t = m_service->getTaskById(id);
        if (!t)
            return makeError("Not found", QHttpServerResponse::StatusCode::NotFound);
        return makeJson(t->toJson());
    });

    // POST /tasks — создать
    server.route("/tasks", QHttpServerRequest::Method::Post,
                 [this](const QHttpServerRequest &req) {
                     QString perr;
                     auto objOpt = parseBodyObject(req, &perr);
                     if (!objOpt)
                         return makeError("Invalid JSON: " + perr, QHttpServerResponse::StatusCode::BadRequest);

                     Task t = Task::fromJson(*objOpt);
                     auto id = m_service->addTask(t);
                     if (id < 0)
                         return makeError("Insert failed", QHttpServerResponse::StatusCode::InternalServerError);

                     t.id = id;
                     return makeJson(t.toJson(), QHttpServerResponse::StatusCode::Created);
                 });

    // PUT /tasks/<int> — обновить (ВАЖНО: сначала id, потом request!)
    server.route("/tasks/<int>", QHttpServerRequest::Method::Put,
                 [this](int id, const QHttpServerRequest &req) {
                     QString perr;
                     auto objOpt = parseBodyObject(req, &perr);
                     if (!objOpt)
                         return makeError("Invalid JSON: " + perr, QHttpServerResponse::StatusCode::BadRequest);

                     Task t = Task::fromJson(*objOpt, id);
                     if (!m_service->updateTask(id, t))
                         return makeError("Update failed or not found", QHttpServerResponse::StatusCode::NotFound);

                     // вернуть актуальную версию
                     auto updated = m_service->getTaskById(id);
                     return makeJson(updated ? updated->toJson() : t.toJson());
                 });

    // DELETE /tasks/<int> — удалить
    server.route("/tasks/<int>", QHttpServerRequest::Method::Delete,
                 [this](int id) {
                     if (!m_service->deleteTask(id))
                         return makeError("Not found", QHttpServerResponse::StatusCode::NotFound);
                     return makeJson(QJsonObject{{"ok", true}});
                 });

    // DELETE /tasks — снести всё
    server.route("/tasks", QHttpServerRequest::Method::Delete,
                 [this] {
                     if (!m_service->deleteAll())
                         return makeError("Delete all failed", QHttpServerResponse::StatusCode::InternalServerError);
                     return makeJson(QJsonObject{{"ok", true}});
                 });
}
