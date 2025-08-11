#include <QtHttpServer/QHttpServerResponse>
#include <QtHttpServer/QHttpServerRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "TaskRouter.hpp"
#include "json_utils.hpp"
#include "Logger.hpp"

TaskRouter::TaskRouter(std::shared_ptr<ITaskService> service)
    : m_service(std::move(service)) {}

void TaskRouter::registerRoutes(QHttpServer &server) {
    // ping
    server.route("/ping", [] {
        qInfo(appHttp) << "PING";
        return QHttpServerResponse("text/plain", "pong");
    });

    // GET /tasks — список (добавим req ради логов)
    server.route("/tasks", QHttpServerRequest::Method::Get,
                 [this](const QHttpServerRequest& req) {
                     qInfo(appHttp) << toString(req.method()) << req.url().toString()
                     << "query:" << req.query().toString();

                     QJsonArray arr;
                     for (const auto &t : m_service->getAllTasks()) {
                         arr.append(t.toJson());
                     }
                     auto resp = makeJsonArray(arr);
                     qInfo(appHttp) << "→ 200 items:" << arr.size();
                     return resp;
                 });

    // GET /tasks/<int>
    server.route("/tasks/<int>", [this](int id) {
        qInfo(appHttp) << "GET /tasks/" << id;
        auto t = m_service->getTaskById(id);
        if (!t) {
            qWarning(appHttp) << "Not found id=" << id;
            return makeError("Not found", QHttpServerResponse::StatusCode::NotFound);
        }
        return makeJson(t->toJson());
    });

    // POST /tasks
    server.route("/tasks", QHttpServerRequest::Method::Post,
                 [this](const QHttpServerRequest &req) {
                     qInfo(appHttp) << "POST /tasks bodyBytes=" << req.body().size();

                     QString perr;
                     auto objOpt = parseBodyObject(req, &perr);
                     if (!objOpt) {
                         qWarning(appHttp) << "Invalid JSON:" << perr;
                         return makeError("Invalid JSON: " + perr, QHttpServerResponse::StatusCode::BadRequest);
                     }

                     Task t = Task::fromJson(*objOpt);
                     auto id = m_service->addTask(t);
                     if (id < 0) {
                         qCritical(appSql) << "Insert failed";
                         return makeError("Insert failed", QHttpServerResponse::StatusCode::InternalServerError);
                     }
                     t.id = id;
                     qInfo(appHttp) << "Created task id=" << id;
                     return makeJson(t.toJson(), QHttpServerResponse::StatusCode::Created);
                 });

    // PUT /tasks/<int>
    server.route("/tasks/<int>", QHttpServerRequest::Method::Put,
                 [this](int id, const QHttpServerRequest &req) {
                     qInfo(appHttp) << "PUT /tasks/" << id << "bodyBytes=" << req.body().size();

                     QString perr;
                     auto objOpt = parseBodyObject(req, &perr);
                     if (!objOpt) {
                         qWarning(appHttp) << "Invalid JSON:" << perr;
                         return makeError("Invalid JSON: " + perr, QHttpServerResponse::StatusCode::BadRequest);
                     }

                     Task t = Task::fromJson(*objOpt, id);
                     if (!m_service->updateTask(id, t)) {
                         qWarning(appHttp) << "Update failed/not found id=" << id;
                         return makeError("Update failed or not found", QHttpServerResponse::StatusCode::NotFound);
                     }

                     auto updated = m_service->getTaskById(id);
                     qInfo(appHttp) << "Updated id=" << id;
                     return makeJson(updated ? updated->toJson() : t.toJson());
                 });

    // DELETE /tasks/<int>
    server.route("/tasks/<int>", QHttpServerRequest::Method::Delete,
                 [this](int id) {
                     qInfo(appHttp) << "DELETE /tasks/" << id;
                     if (!m_service->deleteTask(id)) {
                         qWarning(appHttp) << "Not found id=" << id;
                         return makeError("Not found", QHttpServerResponse::StatusCode::NotFound);
                     }
                     return makeJson(QJsonObject{{"ok", true}});
                 });

    // DELETE /tasks
    server.route("/tasks", QHttpServerRequest::Method::Delete,
                 [this] {
                     qInfo(appHttp) << "DELETE /tasks (all)";
                     if (!m_service->deleteAll()) {
                         qCritical(appSql) << "Delete all failed";
                         return makeError("Delete all failed", QHttpServerResponse::StatusCode::InternalServerError);
                     }
                     return makeJson(QJsonObject{{"ok", true}});
                 });
}
