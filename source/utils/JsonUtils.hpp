#ifndef JSONUTILS_H
#define JSONUTILS_H

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtHttpServer/QHttpServerRequest>
#include <QtHttpServer/QHttpServerResponse>
#include <optional>

#include "Task.hpp"

inline QHttpServerResponse makeJson(const QJsonObject &obj,
                                    QHttpServerResponse::StatusCode status =
                                    QHttpServerResponse::StatusCode::Ok) {
    return QHttpServerResponse(
        "application/json", QJsonDocument(obj).toJson(QJsonDocument::Compact),
        status);
}

inline QHttpServerResponse
makeJsonArray(const QJsonArray &arr, QHttpServerResponse::StatusCode status =
                                     QHttpServerResponse::StatusCode::Ok) {
    return QHttpServerResponse(
        "application/json", QJsonDocument(arr).toJson(QJsonDocument::Compact),
        status);
}

inline QHttpServerResponse makeError(const QString &message,
                                     QHttpServerResponse::StatusCode code) {
    return makeJson(QJsonObject{{"error", message}}, code);
}

inline std::optional<QJsonObject>
parseBodyObject(const QHttpServerRequest &request,
                QString *outError = nullptr) {
    QJsonParseError parseError{};
    const QJsonDocument doc =
        QJsonDocument::fromJson(request.body(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        if (outError) {
            *outError = parseError.errorString();
        }

        return std::nullopt;
    }

    return doc.object();
}

inline bool requireFields(const QJsonObject &obj,
                          std::initializer_list<const char *> keys,
                          QString *missing = nullptr) {
    for (const char *key : keys) {
        if (!obj.contains(key)) {
            if (missing) {
                *missing = key;
            }
            return false;
        }
    }
    return true;
}

inline QJsonObject toJson(const Task &task, bool includeExpanded = false) {
    return task.toJson(includeExpanded);
}

inline Task fromJsonTaskStrict(const QJsonObject &obj,
                               const QUuid &forcedId = QUuid(),
                               QString *outError = nullptr) {
    QString missingKey;
    if (!requireFields(obj, {"title", "description", "isCompleted"},
                       &missingKey)) {
        if (outError) {
            *outError = QString("Missing field: %1").arg(missingKey);
        }
        // не выходим: позволим вернуть Task, caller сам решит, что делать
    }
    return Task::fromJson(obj, forcedId);
}

inline Task patchedTask(const Task &original, const QJsonObject &patch) {
    Task updated = original;

    if (patch.contains("title")) {
        updated.title = patch.value("title").toString();
    }

    if (patch.contains("description")) {
        updated.description = patch.value("description").toString();
    }

    if (patch.contains("isCompleted")) {
        updated.isCompleted =
            patch.value("isCompleted").toBool(updated.isCompleted);
    }

    if (patch.contains("tags") && patch.value("tags").isArray()) {
        const QJsonArray tagsArray = patch.value("tags").toArray();
        QVector<QUuid> newIds;
        newIds.reserve(static_cast<int>(tagsArray.size()));
        for (const QJsonValue &v : tagsArray) {
            QUuid id;
            if (v.isString()) {
                id = QUuid::fromString(v.toString());
            } else if (v.isObject()) {
                id = QUuid::fromString(v.toObject().value("id").toString());
            }
            if (!id.isNull()) {
                newIds.push_back(id);
            }
        }
        updated.tags = std::move(newIds);
    }

    // При патче раскрытые теги больше не валидны
    updated.tagsExpanded.reset();

    return updated;
}

inline QJsonObject paginate(const QJsonArray &data, qsizetype page,
                            qsizetype perPage) {
    const qsizetype total = data.size();
    if (perPage <= 0) {
        perPage = 10;
    }
    if (page <= 0) {
        page = 1;
    }

    const qsizetype totalPages = (total + perPage - 1) / perPage;
    const qsizetype start = (page - 1) * perPage;

    if (start >= total) {
        return QJsonObject{{"page", page},
                           {"per_page", perPage},
                           {"total", total},
                           {"total_pages", totalPages},
                           {"data", QJsonArray{}}};
    }

    QJsonArray slice;

    for (qsizetype i = 0; i < perPage && (start + i) < total; ++i) {
        slice.append(data.at(start + i));
    }

    return QJsonObject{{"page", page},
                       {"per_page", perPage},
                       {"total", total},
                       {"total_pages", totalPages},
                       {"data", slice}};
}

#endif // JSONUTILS_H
