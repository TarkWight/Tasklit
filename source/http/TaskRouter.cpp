#include <QtHttpServer/QHttpServerResponse>
#include <QtHttpServer/QHttpServerRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "TaskRouter.hpp"
#include "json_utils.hpp"
#include "Logger.hpp"
#include "TaskPatch.hpp"

TaskRouter::TaskRouter(std::shared_ptr<ITaskService> service)
    : m_service(std::move(service)) {}

void TaskRouter::registerRoutes(QHttpServer &server) {
    // ping
    server.route("/ping", [] {
        qInfo(appHttp) << "PING";

        return QHttpServerResponse("text/plain", "pong");
        }
    );

    // GET /tasks — список
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
        }
    );

    // GET /tasks/<id>
    server.route("/tasks/<arg>", QHttpServerRequest::Method::Get,
        [this](qint64 id) {
            qInfo(appHttp) << "GET /tasks/" << id;
            auto t = m_service->getTaskById(id);

            if (!t) {
                return makeError("Not found", QHttpServerResponse::StatusCode::NotFound);
            }

            return makeJson(t->toJson());
        }
    );

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
        }
    );

    // PATCH /task/<id>
    server.route("/task/<arg>", QHttpServerRequest::Method::Patch,
        [this](qint64 id, const QHttpServerRequest& req) {
            qInfo(appHttp) << "PATCH /task/" << id << "bodyBytes=" << req.body().size();

            auto current = m_service->getTaskById(id);
            if (!current) {
                qWarning(appHttp) << "Not found id=" << id;

                return makeError("Not found", QHttpServerResponse::StatusCode::NotFound);
            }

            QString perr;
            auto objOpt = parseBodyObject(req, &perr);
            if (!objOpt) {
                qWarning(appHttp) << "Invalid JSON:" << perr;

                return makeError("Invalid JSON: " + perr, QHttpServerResponse::StatusCode::BadRequest);
            }

            Task patched = *current;
            applyTaskPatch(patched, *objOpt);
            if (!m_service->updateTask(id, patched)) {
                qWarning(appHttp) << "Update failed id=" << id;

                return makeError("Update failed", QHttpServerResponse::StatusCode::InternalServerError);
            }

            auto updated = m_service->getTaskById(id);

            return makeJson(updated ? updated->toJson() : patched.toJson());
        }
    );

    // DELETE /task?id=<id>
    server.route("/task", QHttpServerRequest::Method::Delete,
        [this](const QHttpServerRequest& req) {
            const auto q = req.query();
            const auto idStr = q.queryItemValue("id");

            if (!idStr.isEmpty()) {
                bool ok = false;
                const qint64 id = idStr.toLongLong(&ok);
                if (!ok) {
                    return makeError("Invalid id", QHttpServerResponse::StatusCode::BadRequest);
                }

                qInfo(appHttp) << "DELETE /task id=" << id;

                if (!m_service->deleteTask(id)) {
                    return makeError("Task with id<" + idStr + "> not found!", QHttpServerResponse::StatusCode::NotFound);
                }

                return makeJson(QJsonObject{{"Ok", true}});
            }

            return makeJson(QJsonObject{{"Ok", true}});
        }
    );

    // DELETE /tasks
    server.route("/tasks", QHttpServerRequest::Method::Delete,
        [this] {
            qInfo(appHttp) << "DELETE /tasks (all)";
            if (!m_service->deleteAll()) {
                qCritical(appSql) << "Delete all failed";

                return makeError("Delete all failed", QHttpServerResponse::StatusCode::InternalServerError);
            }

        return makeJson(QJsonObject{ { "ok", true } });
        }
    );

    // GET /tags
    server.route("/tags", QHttpServerRequest::Method::Get,
        [this](const QHttpServerRequest &req) {
            qInfo(appHttp) << "GET /tags";
            QJsonArray arr;

            for (const auto &tag : m_service->getAllTags()) {
                arr.append(tag.toJson());
            }

            return makeJsonArray(arr);
        }
    );

    // POST /tags
    server.route("/tags", QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest &req) {
            qInfo(appHttp) << "POST /tags bodyBytes=" << req.body().size();

            QString perr;
            auto objOpt = parseBodyObject(req, &perr);

            if (!objOpt) {
                qWarning(appHttp) << "Invalid JSON:" << perr;

                return makeError("Invalid JSON: " + perr, QHttpServerResponse::StatusCode::BadRequest);
            }

            Tag tag = Tag::fromJson(*objOpt);
            auto id = m_service->addTag(tag);
            if (id < 0) {
                qCritical(appSql) << "Insert tag failed";

                return makeError("Insert tag failed", QHttpServerResponse::StatusCode::InternalServerError);
            }

            tag.id = id;
            qInfo(appHttp) << "Created tag id=" << id;

            return makeJson(tag.toJson(), QHttpServerResponse::StatusCode::Created);
        }
    );

    // Error and empty request handler
    server.setMissingHandler(&server,
        [](const QHttpServerRequest &req, QHttpServerResponder &res) {
            qWarning(appHttp) << "404 no route for"
            << toString(req.method()) << req.url().toString();

        QJsonObject obj{
            { "error",  "Not found" },
            { "path",   req.url().toString() },
            { "method", toString(req.method()) }
        };

        QHttpServerResponse r(obj, QHttpServerResponse::StatusCode::NotFound);
        res.sendResponse(r);
        }
    );
}
