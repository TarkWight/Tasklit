#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtHttpServer/QHttpServerRequest>
#include <QtHttpServer/QHttpServerResponse>

#include "Logger.hpp"
#include "TaskPatch.hpp"
#include "TaskRouter.hpp"
#include "json_utils.hpp"

TaskRouter::TaskRouter(std::shared_ptr<ITaskService> service)
    : m_service(std::move(service)) {}

void TaskRouter::registerRoutes(QHttpServer &server) {
    // ─────────────────────────────────────────────────────────────────────────────
    // Локальные хелперы (только для этого метода)
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

    const auto respondOk = [](const char *message) {
        return makeJson(
            QJsonObject{{"ok", true}, {"message", QString::fromUtf8(message)}});
    };

    // дублирует маршрут со слэшем и без ("/x" и "/x/")
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
    // TASKS: чтение списка
    // GET /tasks  (всё)
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute("/tasks", QHttpServerRequest::Method::Get,
                [this](const QHttpServerRequest &request) {
                    qInfo(appHttp) << "[GET] /tasks"
                                   << "url:" << request.url().toString()
                                   << "query:" << request.query().toString();

                    QJsonArray tasksJson;
                    const auto allTasks = m_service->getAllTasks();
                    for (const Task &task : allTasks)
                        tasksJson.append(task.toJson());

                    qInfo(appHttp) << "→ 200 OK | items:" << tasksJson.size();
                    return makeJsonArray(tasksJson);
                });

    // ─────────────────────────────────────────────────────────────────────────────
    // TASK: чтение одной задачи по ?id
    // GET /task?id=<id>
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/task", QHttpServerRequest::Method::Get,
        [this, &parseIdFromQuery](const QHttpServerRequest &request) {
            qInfo(appHttp) << "[GET] /task"
                           << "url:" << request.url().toString();

            qint64 taskId = -1;
            QString parseError;
            if (!parseIdFromQuery(request, taskId, parseError)) {
                qWarning(appHttp) << "→ 400 Bad Request |" << parseError;
                return makeError(parseError,
                                 QHttpServerResponse::StatusCode::BadRequest);
            }

            const auto taskOpt = m_service->getTaskById(taskId);
            if (!taskOpt) {
                const QString msg =
                    QStringLiteral("Task with id=%1 not found").arg(taskId);
                qWarning(appHttp) << "→ 404 Not Found |" << msg;
                return makeError(msg,
                                 QHttpServerResponse::StatusCode::NotFound);
            }

            qInfo(appHttp) << "→ 200 OK | id=" << taskId;
            return makeJson(taskOpt->toJson());
        });

    // ─────────────────────────────────────────────────────────────────────────────
    // TASK: создание
    // POST /task/create
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/task/create", QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest &request) {
            qInfo(appHttp) << "[POST] /task/create"
                           << "bodyBytes=" << request.body().size();

            QString parseError;
            const auto bodyObjectOpt = parseBodyObject(request, &parseError);
            if (!bodyObjectOpt) {
                qWarning(appHttp)
                << "→ 400 Bad Request | Invalid JSON:" << parseError;
                return makeError("Invalid JSON: " + parseError,
                                 QHttpServerResponse::StatusCode::BadRequest);
            }

            Task newTask = Task::fromJson(*bodyObjectOpt);
            const qint64 newId = m_service->addTask(newTask);
            if (newId < 0) {
                qCritical(appSql)
                << "→ 500 Internal Server Error | Insert failed";
                return makeError(
                    "Insert failed",
                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            newTask.id = newId;
            qInfo(appHttp) << "→ 201 Created | id=" << newId;
            return makeJson(newTask.toJson(),
                            QHttpServerResponse::StatusCode::Created);
        });

    // ─────────────────────────────────────────────────────────────────────────────
    // TASK: частичное обновление
    // PATCH /task/<id>
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/task/<arg>", QHttpServerRequest::Method::Patch,
        [this](qint64 taskId, const QHttpServerRequest &request) {
            qInfo(appHttp) << "[PATCH] /task/" << taskId
                           << "bodyBytes=" << request.body().size();

            const auto currentOpt = m_service->getTaskById(taskId);
            if (!currentOpt) {
                const QString msg =
                    QStringLiteral("Task with id=%1 not found").arg(taskId);
                qWarning(appHttp) << "→ 404 Not Found |" << msg;
                return makeError(msg,
                                 QHttpServerResponse::StatusCode::NotFound);
            }

            QString parseError;
            const auto bodyObjectOpt = parseBodyObject(request, &parseError);
            if (!bodyObjectOpt) {
                qWarning(appHttp)
                << "→ 400 Bad Request | Invalid JSON:" << parseError;
                return makeError("Invalid JSON: " + parseError,
                                 QHttpServerResponse::StatusCode::BadRequest);
            }

            Task patchedTask = *currentOpt;
            applyTaskPatch(patchedTask, *bodyObjectOpt);

            if (!m_service->updateTask(taskId, patchedTask)) {
                qWarning(appHttp)
                << "→ 500 Internal Server Error | Update failed id="
                << taskId;
                return makeError(
                    "Update failed",
                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            const auto updatedOpt = m_service->getTaskById(taskId);
            qInfo(appHttp) << "→ 200 OK | Updated id=" << taskId;
            return makeJson(updatedOpt ? updatedOpt->toJson()
                                       : patchedTask.toJson());
        });

    // ─────────────────────────────────────────────────────────────────────────────
    // TASK: удаление одной
    // DELETE /task?id=<id>
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/task", QHttpServerRequest::Method::Delete,
        [this, &parseIdFromQuery,
         &respondOk](const QHttpServerRequest &request) {
            qInfo(appHttp) << "[DELETE] /task"
                           << "url:" << request.url().toString();

            qint64 taskId = -1;
            QString parseError;
            if (!parseIdFromQuery(request, taskId, parseError)) {
                qWarning(appHttp) << "→ 400 Bad Request |" << parseError;
                return makeError(parseError,
                                 QHttpServerResponse::StatusCode::BadRequest);
            }

            if (!m_service->deleteTask(taskId)) {
                const QString msg =
                    QStringLiteral("Task with id=%1 not found").arg(taskId);
                qWarning(appHttp) << "→ 404 Not Found |" << msg;
                return makeError(msg,
                                 QHttpServerResponse::StatusCode::NotFound);
            }

            qInfo(appHttp) << "→ 200 OK | Deleted id=" << taskId;
            return respondOk("Task deleted");
        });

    // ─────────────────────────────────────────────────────────────────────────────
    // TASKS: удаление всех
    // DELETE /tasks
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/tasks", QHttpServerRequest::Method::Delete, [this, &respondOk] {
            qInfo(appHttp) << "[DELETE] /tasks (all)";

            if (!m_service->deleteAll()) {
                qCritical(appSql)
                << "→ 500 Internal Server Error | Delete all failed";
                return makeError(
                    "Delete all failed",
                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            qInfo(appHttp) << "→ 200 OK | All tasks deleted";
            return respondOk("All tasks deleted");
        });

    // ─────────────────────────────────────────────────────────────────────────────
    // TAGS: список
    // GET /tags
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute("/tags", QHttpServerRequest::Method::Get,
                [this](const QHttpServerRequest & /*request*/) {
                    qInfo(appHttp) << "[GET] /tags";

                    QJsonArray tagsJson;
                    const auto allTags = m_service->getAllTags();
                    for (const Tag &tag : allTags)
                        tagsJson.append(tag.toJson());

                    qInfo(appHttp) << "→ 200 OK | items:" << tagsJson.size();
                    return makeJsonArray(tagsJson);
                });

    // ─────────────────────────────────────────────────────────────────────────────
    // TAGS: создание
    // POST /tags
    // ─────────────────────────────────────────────────────────────────────────────
    mirrorRoute(
        "/tag/create", QHttpServerRequest::Method::Post,
        [this](const QHttpServerRequest &request) {
            qInfo(appHttp) << "[POST] /tags"
                           << "bodyBytes=" << request.body().size();

            QString parseError;
            const auto bodyObjectOpt = parseBodyObject(request, &parseError);
            if (!bodyObjectOpt) {
                qWarning(appHttp)
                << "→ 400 Bad Request | Invalid JSON:" << parseError;
                return makeError("Invalid JSON: " + parseError,
                                 QHttpServerResponse::StatusCode::BadRequest);
            }

            Tag newTag = Tag::fromJson(*bodyObjectOpt);
            const qint64 newId = m_service->addTag(newTag);
            if (newId < 0) {
                qCritical(appSql)
                << "→ 500 Internal Server Error | Insert tag failed";
                return makeError(
                    "Insert tag failed",
                    QHttpServerResponse::StatusCode::InternalServerError);
            }

            newTag.id = newId;
            qInfo(appHttp) << "→ 201 Created | tag id=" << newId;
            return makeJson(newTag.toJson(),
                            QHttpServerResponse::StatusCode::Created);
        });

    // ─────────────────────────────────────────────────────────────────────────────
    // Fallback 404
    // ─────────────────────────────────────────────────────────────────────────────
    server.setMissingHandler(&server, [](const QHttpServerRequest &request,
                                         QHttpServerResponder &responder) {
        const QString methodString = toString(request.method());
        const QString urlString = request.url().toString();
        qWarning(appHttp) << "404 no route for" << methodString << urlString;

        QJsonObject errorJson{
                              {"error", "Not found"},
                              {"path", urlString},
                              {"method", methodString},
                              {"hint", "Check path, HTTP method and trailing slash"}};

        QHttpServerResponse response(errorJson,
                                     QHttpServerResponse::StatusCode::NotFound);
        responder.sendResponse(response);
    });
}
