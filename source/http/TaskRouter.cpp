#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>
#include <QtHttpServer/QHttpServerRequest>
#include <QtHttpServer/QHttpServerResponse>

#include "ErrorHandler.hpp"
#include "JsonUtils.hpp"
#include "Logger.hpp"
#include "Tag.hpp"
#include "Task.hpp"
#include "TaskPatch.hpp"
#include "TaskRouter.hpp"

TaskRouter::TaskRouter(std::shared_ptr<ITaskService> service)
    : m_service(std::move(service)) {}

static QUuid parseUuidLoose(const QString &string) {
    QUuid id = QUuid::fromString(string);
    if (!id.isNull()) {
        return id;
    }
    if (!string.isEmpty() && !string.startsWith('{') && !string.endsWith('}')) {
        id = QUuid::fromString(QStringLiteral("{") + string +
                               QStringLiteral("}"));
    }
    return id;
}

void TaskRouter::registerRoutes(QHttpServer &server) {
    // ─────────────────────────────────────────────────────────────────────────────
    // Helpers
    // ─────────────────────────────────────────────────────────────────────────────
    const auto parseUuidFromQuery = [](const QHttpServerRequest &request,
                                       QUuid &outId,
                                       QString &outError) -> bool {
        const QUrlQuery query = request.query();

        const QString idString = query.queryItemValue(QStringLiteral("id"));
        if (idString.isEmpty()) {
            outError = QStringLiteral("Missing 'id' query param");
            return false;
        }

        const QUuid id = parseUuidLoose(idString);
        if (id.isNull()) {
            outError = QStringLiteral("Invalid 'id' (expected UUID)");
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
        if (!withSlash.endsWith('/')) {
            withSlash.append('/');
        }

        server.route(withSlash.toLatin1().constData(), method, handler);
    };

    // ─────────────────────────────────────────────────────────────────────────────
    // GET /tasks
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/tasks", QHttpServerRequest::Method::Get,
        wrapSafe("GET /tasks",
                 std::function<QHttpServerResponse(const QHttpServerRequest &,
                                                   const QString &)>(
                     [this](const QHttpServerRequest &request,
                            const QString &requestId) {
                         qInfo(appHttp)
                         << "[GET] /tasks"
                         << "url:" << request.url().toString()
                         << "query:" << request.query().toString()
                         << "| requestId=" << requestId;

                         QJsonArray items;
                         const auto allTasks = m_service->getAllTasks();
                         for (const Task &task : allTasks) {
                             items.append(task.toJson());
                         }

                         return makeApiOk("Tasks fetched",
                                          QJsonObject{{"items", items},
                                                      {"count", items.size()}},
                                          requestId);
                     })));

    // ─────────────────────────────────────────────────────────────────────────────
    // GET /task?id=<uuid>
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/task", QHttpServerRequest::Method::Get,
        wrapSafe(
            "GET /task",
            std::function<QHttpServerResponse(const QHttpServerRequest &,
                                              const QString &)>(
                [this, &parseUuidFromQuery](const QHttpServerRequest &request,
                                            const QString &requestId) {
                    qInfo(appHttp) << "[GET] /task"
                                   << "url:" << request.url().toString()
                                   << "| requestId=" << requestId;

                    QUuid taskId;
                    QString parseError;
                    if (!parseUuidFromQuery(request, taskId, parseError)) {
                        return makeApiError(
                            QHttpServerResponse::StatusCode::BadRequest,
                            parseError, "bad_request", {}, requestId);
                    }

                    const auto taskOpt = m_service->getTaskById(taskId);
                    if (!taskOpt) {
                        return makeApiError(
                            QHttpServerResponse::StatusCode::NotFound,
                            QString("Task with id=%1 not found")
                                .arg(taskId.toString(QUuid::WithoutBraces)),
                            "not_found",
                            QJsonObject{
                                        {"id", taskId.toString(QUuid::WithoutBraces)}},
                            requestId);
                    }

                    return makeApiOk("Task fetched",
                                     QJsonObject{{"task", taskOpt->toJson()}},
                                     requestId);
                })));

    // ─────────────────────────────────────────────────────────────────────────────
    // POST /task/create
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/task/create", QHttpServerRequest::Method::Post,
        wrapSafe(
            "POST /task/create",
            std::function<QHttpServerResponse(
                const QHttpServerRequest &,
                const QString &)>([this](const QHttpServerRequest &request,
                                         const QString &requestId) {
                qInfo(appHttp) << "[POST] /task/create"
                               << "bytes=" << request.body().size()
                               << "| requestId=" << requestId;

                QString parseError;
                const auto bodyOpt = parseBodyObject(request, &parseError);
                if (!bodyOpt) {
                    return makeApiError(
                        QHttpServerResponse::StatusCode::BadRequest,
                        "Invalid JSON: " + parseError, "bad_request", {},
                        requestId);
                }

                QJsonObject payload = *bodyOpt;

                const QString title = payload.value("title").toString();
                if (title.trimmed().isEmpty()) {
                    return makeApiError(
                        QHttpServerResponse::StatusCode::BadRequest,
                        "Field 'title' is required and must be non-empty",
                        "validation_error", QJsonObject{{"field", "title"}},
                        requestId);
                }

                if (!payload.contains("description")) {
                    payload.insert("description", "");
                }
                if (!payload.contains("isCompleted")) {
                    payload.insert("isCompleted", false);
                }

                QVector<QUuid> tagIds;
                if (payload.contains("tags")) {
                    const QJsonValue tagsVal = payload.value("tags");
                    if (!tagsVal.isArray()) {
                        return makeApiError(
                            QHttpServerResponse::StatusCode::BadRequest,
                            "Field 'tags' must be an array", "validation_error",
                            QJsonObject{{"field", "tags"}}, requestId);
                    }

                    const QJsonArray in = tagsVal.toArray();
                    tagIds.reserve(in.size());
                    for (const QJsonValue &v : in) {
                        QUuid parsed;
                        if (v.isString()) {
                            parsed = parseUuidLoose(v.toString());
                        } else if (v.isObject()) {
                            parsed = parseUuidLoose(
                                v.toObject().value("id").toString());
                        } else {
                            return makeApiError(
                                QHttpServerResponse::StatusCode::BadRequest,
                                "Each tag must be a UUID string or an object "
                                "with 'id' (UUID)",
                                "validation_error",
                                QJsonObject{{"field", "tags"}}, requestId);
                        }

                        if (parsed.isNull()) {
                            return makeApiError(
                                QHttpServerResponse::StatusCode::BadRequest,
                                "Invalid tag id (expected UUID)",
                                "validation_error",
                                QJsonObject{{"field", "tags"}}, requestId);
                        }
                        tagIds.push_back(parsed);
                    }
                }

                Task newTask = Task::fromJson(payload);
                newTask.tags = std::move(tagIds);

                const QUuid storedId = m_service->addTask(newTask);
                if (storedId.isNull()) {
                    return makeApiError(
                        QHttpServerResponse::StatusCode::InternalServerError,
                        "Insert failed", "internal_error", {}, requestId);
                }
                newTask.id = storedId;

                return makeApiOk(
                    "Task created", QJsonObject{{"task", newTask.toJson()}},
                    requestId, QHttpServerResponse::StatusCode::Created);
            })));

    // ─────────────────────────────────────────────────────────────────────────────
    // PATCH /task?id<uuid>
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/task", QHttpServerRequest::Method::Patch,
        wrapSafe(
            "PATCH /task",
            std::function<QHttpServerResponse(
                const QHttpServerRequest &,
                const QString &)>([this, &parseUuidFromQuery](
                                      const QHttpServerRequest &request,
                                      const QString &requestId) {
                qInfo(appHttp) << "[PATCH] /task"
                               << "url:" << request.url().toString()
                               << "bytes=" << request.body().size()
                               << "| requestId=" << requestId;

                QUuid taskId;
                QString parseError;
                if (!parseUuidFromQuery(request, taskId, parseError)) {
                    return makeApiError(QHttpServerResponse::StatusCode::BadRequest,
                                        parseError, "bad_request", {}, requestId);
                }

                const auto current = m_service->getTaskById(taskId);
                if (!current) {
                    return makeApiError(
                        QHttpServerResponse::StatusCode::NotFound,
                        QString("Task with id=%1 not found")
                            .arg(taskId.toString(QUuid::WithoutBraces)),
                        "not_found",
                        QJsonObject{{"id", taskId.toString(QUuid::WithoutBraces)}},
                        requestId);
                }

                QString bodyErr;
                const auto body = parseBodyObject(request, &bodyErr);
                if (!body) {
                    return makeApiError(QHttpServerResponse::StatusCode::BadRequest,
                                        "Invalid JSON: " + bodyErr, "bad_request",
                                        {}, requestId);
                }

                Task patched = *current;
                applyTaskPatch(
                    patched,
                    *body);

                if (!m_service->updateTask(taskId, patched)) {
                    return makeApiError(
                        QHttpServerResponse::StatusCode::InternalServerError,
                        "Update failed", "internal_error",
                        QJsonObject{{"id", taskId.toString(QUuid::WithoutBraces)}},
                        requestId);
                }

                const auto updated = m_service->getTaskById(taskId);
                return makeApiOk(
                    "Task updated",
                    QJsonObject{
                                {"task", (updated ? updated->toJson() : patched.toJson())}},
                    requestId);
            })));


    // ─────────────────────────────────────────────────────────────────────────────
    // DELETE /task?id=<uuid>
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/task", QHttpServerRequest::Method::Delete,
        wrapSafe(
            "DELETE /task",
            std::function<QHttpServerResponse(const QHttpServerRequest &,
                                              const QString &)>(
                [this, &parseUuidFromQuery](const QHttpServerRequest &request,
                                            const QString &requestId) {
                    qInfo(appHttp) << "[DELETE] /task"
                                   << "url:" << request.url().toString()
                                   << "| requestId=" << requestId;

                    QUuid taskId;
                    QString parseError;
                    if (!parseUuidFromQuery(request, taskId, parseError)) {
                        return makeApiError(
                            QHttpServerResponse::StatusCode::BadRequest,
                            parseError, "bad_request", {}, requestId);
                    }

                    if (!m_service->deleteTask(taskId)) {
                        return makeApiError(
                            QHttpServerResponse::StatusCode::NotFound,
                            "Task not found", "not_found",
                            QJsonObject{
                                        {"id", taskId.toString(QUuid::WithoutBraces)}},
                            requestId);
                    }

                    return makeApiOk(
                        "Task deleted",
                        QJsonObject{
                                    {"id", taskId.toString(QUuid::WithoutBraces)}},
                        requestId);
                })));

    // ─────────────────────────────────────────────────────────────────────────────
    // DELETE /tasks
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/tasks", QHttpServerRequest::Method::Delete,
        wrapSafe(
            "DELETE /tasks",
            std::function<QHttpServerResponse(
                const QString &)>([this](const QString &requestId) {
                qInfo(appHttp) << "[DELETE] /tasks (all)"
                               << "| requestId=" << requestId;

                if (!m_service->deleteAll()) {
                    return makeApiError(
                        QHttpServerResponse::StatusCode::InternalServerError,
                        "Delete all failed", "internal_error", {}, requestId);
                }

                return makeApiOk("All tasks deleted", {}, requestId);
            })));

    // ─────────────────────────────────────────────────────────────────────────────
    // GET /tags
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute("/tags", QHttpServerRequest::Method::Get,
                wrapSafe("GET /tags",
                         std::function<QHttpServerResponse(const QString &)>(
                             [this](const QString &requestId) {
                                 qInfo(appHttp) << "[GET] /tags"
                                                << "| requestId=" << requestId;

                                 QJsonArray items;
                                 const auto allTags = m_service->getAllTags();
                                 for (const Tag &tag : allTags) {
                                     items.append(tag.toJson());
                                 }

                                 return makeApiOk(
                                     "Tags fetched",
                                     QJsonObject{{"items", items},
                                                 {"count", items.size()}},
                                     requestId);
                             })));

    // ─────────────────────────────────────────────────────────────────────────────
    // POST /tag/create
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/tag/create", QHttpServerRequest::Method::Post,
        wrapSafe(
            "POST /tag/create",
            std::function<QHttpServerResponse(
                const QHttpServerRequest &,
                const QString &)>([this](const QHttpServerRequest &request,
                                         const QString &requestId) {
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
                if (tag.name.trimmed().isEmpty()) {
                    return makeApiError(
                        QHttpServerResponse::StatusCode::BadRequest,
                        "Field 'name' is required and must be non-empty",
                        "validation_error", QJsonObject{{"field", "name"}},
                        requestId);
                }

                const QUuid newId = m_service->addTag(tag);
                if (newId.isNull()) {
                    return makeApiError(
                        QHttpServerResponse::StatusCode::InternalServerError,
                        "Insert tag failed", "internal_error", {}, requestId);
                }

                tag.id = newId;
                return makeApiOk("Tag created",
                                 QJsonObject{{"tag", tag.toJson()}}, requestId,
                                 QHttpServerResponse::StatusCode::Created);
            })));

    // ─────────────────────────────────────────────────────────────────────────────
    // Глобальный 404‑фолбек
    // ─────────────────────────────────────────────────────────────────────────────
    server.setMissingHandler(&server, [](const QHttpServerRequest &request,
                                         QHttpServerResponder &responder) {
        sendNotFound(responder, request);
    });
}
