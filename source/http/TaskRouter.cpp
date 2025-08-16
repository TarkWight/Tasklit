#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtHttpServer/QHttpServerRequest>
#include <QtHttpServer/QHttpServerResponse>

#include "ErrorHandler.hpp"
#include "Logger.hpp"
#include "TaskPatch.hpp"
#include "TaskRouter.hpp"
#include "json_utils.hpp"

TaskRouter::TaskRouter(std::shared_ptr<ITaskService> service)
    : m_service(std::move(service)) {}

void TaskRouter::registerRoutes(QHttpServer &server) {
    // ─────────────────────────────────────────────────────────────────────────────
    // Локальные хелперы
    // ─────────────────────────────────────────────────────────────────────────────
    const auto parseIdFromQuery = [](const QHttpServerRequest &request,
                                     qint64 &outId, QString &outError) -> bool {
        const QUrlQuery query = request.query();
        const QString idString = query.queryItemValue(QStringLiteral("id"));
        if (idString.isEmpty()) {
            outError = QStringLiteral("Missing 'id' query param");
            return false;
        }
        bool ok = false;
        const qint64 id = idString.toLongLong(&ok);
        if (!ok) {
            outError = QStringLiteral("Invalid 'id' (expected integer)");
            return false;
        }
        outId = id;
        return true;
    };

    const auto mirrorRoute = [&server](const char *path,
                                       QHttpServerRequest::Method method,
                                       auto handler) {
        server.route(path, method, handler);
        QString withSlash = QString::fromLatin1(path);
        if (!withSlash.endsWith('/'))
            withSlash.append('/');
        server.route(withSlash.toLatin1().constData(), method, handler);
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // GET /tasks
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/tasks", QHttpServerRequest::Method::Get,
        wrapSafe("GET /tasks",
                 [this](const QHttpServerRequest &request,
                        const QString &requestId) -> QHttpServerResponse {
                     qInfo(appHttp) << "[GET] /tasks"
                                    << "url:" << request.url().toString()
                                    << "query:" << request.query().toString()
                                    << "| requestId=" << requestId;

                     QJsonArray items;
                     const auto allTasks = m_service->getAllTasks();
                     for (const Task &task : allTasks)
                         items.append(task.toJson());

                     QJsonObject data{{"items", items},
                                      {"count", items.size()}};
                     return makeApiOk("Tasks fetched", data, requestId);
                 }));

    // ─────────────────────────────────────────────────────────────────────────────
    // GET /task?id=<id>
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/task", QHttpServerRequest::Method::Get,
        wrapSafe("GET /task",
                 [this, &parseIdFromQuery](
                     const QHttpServerRequest &request,
                     const QString &requestId) -> QHttpServerResponse {
                     qInfo(appHttp) << "[GET] /task"
                                    << "url:" << request.url().toString()
                                    << "| requestId=" << requestId;

                     qint64 taskId = -1;
                     QString parseError;
                     if (!parseIdFromQuery(request, taskId, parseError)) {
                         return makeApiError(
                             QHttpServerResponse::StatusCode::BadRequest,
                             parseError, "bad_request", {}, requestId);
                     }

                     const auto taskOpt = m_service->getTaskById(taskId);
                     if (!taskOpt) {
                         return makeApiError(
                             QHttpServerResponse::StatusCode::NotFound,
                             QString("Task with id=%1 not found").arg(taskId),
                             "not_found",
                             QJsonObject{{"id", QString::number(taskId)}},
                             requestId);
                     }

                     return makeApiOk("Task fetched",
                                      QJsonObject{{"task", taskOpt->toJson()}},
                                      requestId);
                 }));

    // ─────────────────────────────────────────────────────────────────────────────
    // POST /task/create
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/task/create", QHttpServerRequest::Method::Post,
        wrapSafe(
            "POST /task/create",
            [this](const QHttpServerRequest &request,
                   const QString &requestId) -> QHttpServerResponse {
                qInfo(appHttp) << "[POST] /task/create"
                               << "bytes=" << request.body().size()
                               << "| requestId=" << requestId;

                QString parseError;
                const auto body = parseBodyObject(request, &parseError);
                if (!body) {
                    return makeApiError(
                        QHttpServerResponse::StatusCode::BadRequest,
                        "Invalid JSON: " + parseError, "bad_request", {},
                        requestId);
                }

                Task newTask = Task::fromJson(*body);
                const qint64 newId = m_service->addTask(newTask);
                if (newId < 0) {
                    return makeApiError(
                        QHttpServerResponse::StatusCode::InternalServerError,
                        "Insert failed", "internal_error", {}, requestId);
                }

                newTask.id = newId;
                QHttpServerResponse resp(
                    QJsonObject{
                                {"ok", true},
                                {"message", "Task created"},
                                {"requestId", requestId},
                                {"ts", QDateTime::currentDateTimeUtc().toString(
                                           Qt::ISODateWithMs)},
                                {"data", QJsonObject{{"task", newTask.toJson()}}}},
                    QHttpServerResponse::StatusCode::Created);
                return resp;
            }));

    // ─────────────────────────────────────────────────────────────────────────────
    // PATCH /task/<id>
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/task/<arg>", QHttpServerRequest::Method::Patch,
        wrapSafe(
            "PATCH /task/<id>",
            [this](qint64 taskId, const QHttpServerRequest &request,
                   const QString &requestId) -> QHttpServerResponse {
                qInfo(appHttp) << "[PATCH] /task/" << taskId
                               << "bytes=" << request.body().size()
                               << "| requestId=" << requestId;

                const auto current = m_service->getTaskById(taskId);
                if (!current) {
                    return makeApiError(
                        QHttpServerResponse::StatusCode::NotFound,
                        QString("Task with id=%1 not found").arg(taskId),
                        "not_found",
                        QJsonObject{{"id", QString::number(taskId)}},
                        requestId);
                }

                QString parseError;
                const auto body = parseBodyObject(request, &parseError);
                if (!body) {
                    return makeApiError(
                        QHttpServerResponse::StatusCode::BadRequest,
                        "Invalid JSON: " + parseError, "bad_request", {},
                        requestId);
                }

                Task patched = *current;
                applyTaskPatch(patched, *body);
                if (!m_service->updateTask(taskId, patched)) {
                    return makeApiError(
                        QHttpServerResponse::StatusCode::InternalServerError,
                        "Update failed", "internal_error",
                        QJsonObject{{"id", QString::number(taskId)}},
                        requestId);
                }

                const auto updated = m_service->getTaskById(taskId);
                return makeApiOk(
                    "Task updated",
                    QJsonObject{{"task", (updated ? updated->toJson()
                                                  : patched.toJson())}},
                    requestId);
            }));

    // ─────────────────────────────────────────────────────────────────────────────
    // DELETE /task?id=<id>
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/task", QHttpServerRequest::Method::Delete,
        wrapSafe("DELETE /task",
                 [this, &parseIdFromQuery](
                     const QHttpServerRequest &request,
                     const QString &requestId) -> QHttpServerResponse {
                     qInfo(appHttp) << "[DELETE] /task"
                                    << "url:" << request.url().toString()
                                    << "| requestId=" << requestId;

                     qint64 taskId = -1;
                     QString parseError;
                     if (!parseIdFromQuery(request, taskId, parseError)) {
                         return makeApiError(
                             QHttpServerResponse::StatusCode::BadRequest,
                             parseError, "bad_request", {}, requestId);
                     }

                     if (!m_service->deleteTask(taskId)) {
                         return makeApiError(
                             QHttpServerResponse::StatusCode::NotFound,
                             "Task not found", "not_found",
                             QJsonObject{{"id", QString::number(taskId)}},
                             requestId);
                     }

                     return makeApiOk(
                         "Task deleted",
                         QJsonObject{{"id", QString::number(taskId)}},
                         requestId);
                 }));

    // ─────────────────────────────────────────────────────────────────────────────
    // DELETE /tasks
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/tasks", QHttpServerRequest::Method::Delete,
        wrapSafe(
            "DELETE /tasks",
            [this](const QString &requestId) -> QHttpServerResponse {
                qInfo(appHttp) << "[DELETE] /tasks (all)"
                               << "| requestId=" << requestId;

                if (!m_service->deleteAll()) {
                    return makeApiError(
                        QHttpServerResponse::StatusCode::InternalServerError,
                        "Delete all failed", "internal_error", {}, requestId);
                }

                return makeApiOk("All tasks deleted", {}, requestId);
            }));

    // ─────────────────────────────────────────────────────────────────────────────
    // GET /tags
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/tags", QHttpServerRequest::Method::Get,
        wrapSafe("GET /tags",
                 [this](const QString &requestId) -> QHttpServerResponse {
                     qInfo(appHttp) << "[GET] /tags"
                                    << "| requestId=" << requestId;

                     QJsonArray items;
                     const auto allTags = m_service->getAllTags();
                     for (const Tag &t : allTags)
                         items.append(t.toJson());

                     return makeApiOk(
                         "Tags fetched",
                         QJsonObject{{"items", items}, {"count", items.size()}},
                         requestId);
                 }));

    // ─────────────────────────────────────────────────────────────────────────────
    // POST /tag/create
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/tag/create", QHttpServerRequest::Method::Post,
        wrapSafe(
            "POST /tag/create",
            [this](const QHttpServerRequest &request,
                   const QString &requestId) -> QHttpServerResponse {
                qInfo(appHttp) << "[POST] /tag/create"
                               << "bytes=" << request.body().size()
                               << "| requestId=" << requestId;

                QString parseError;
                const auto body = parseBodyObject(request, &parseError);
                if (!body) {
                    return makeApiError(
                        QHttpServerResponse::StatusCode::BadRequest,
                        "Invalid JSON: " + parseError, "bad_request", {},
                        requestId);
                }

                Tag tag = Tag::fromJson(*body);
                const qint64 newId = m_service->addTag(tag);
                if (newId < 0) {
                    return makeApiError(
                        QHttpServerResponse::StatusCode::InternalServerError,
                        "Insert tag failed", "internal_error", {}, requestId);
                }

                tag.id = newId;
                QHttpServerResponse resp(
                    QJsonObject{{"ok", true},
                                {"message", "Tag created"},
                                {"requestId", requestId},
                                {"ts", QDateTime::currentDateTimeUtc().toString(
                                           Qt::ISODateWithMs)},
                                {"data", QJsonObject{{"tag", tag.toJson()}}}},
                    QHttpServerResponse::StatusCode::Created);
                return resp;
            }));

    // ─────────────────────────────────────────────────────────────────────────────
    // Глобальный 404‑фолбек
    // ─────────────────────────────────────────────────────────────────────────────
    server.setMissingHandler(&server, [](const QHttpServerRequest &request,
                                         QHttpServerResponder &responder) {
        sendNotFound(responder, request);
    });
}
