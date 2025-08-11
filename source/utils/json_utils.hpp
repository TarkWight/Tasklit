#ifndef JSON_UTILS_HPP
#define JSON_UTILS_HPP

#include <QtHttpServer/QHttpServerResponse>
#include <QtHttpServer/QHttpServerRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <optional>

#include "task.hpp"
#include "tag.hpp"


inline QHttpServerResponse makeJson(const QJsonObject& obj,
                                    QHttpServerResponse::StatusCode status =
                                    QHttpServerResponse::StatusCode::Ok
                                    ) {
    return QHttpServerResponse(
        "application/json",
        QJsonDocument(obj).toJson(QJsonDocument::Compact),
        status
        );
}

inline QHttpServerResponse makeJsonArray(const QJsonArray& arr,
                                         QHttpServerResponse::StatusCode status =
                                         QHttpServerResponse::StatusCode::Ok
                                         ) {
    return QHttpServerResponse(
        "application/json",
        QJsonDocument(arr).toJson(QJsonDocument::Compact),
        status
        );
}

inline QHttpServerResponse makeError(const QString& msg,
                                     QHttpServerResponse::StatusCode code
                                     ) {
    return makeJson(QJsonObject{{"error", msg}}, code);
}

inline std::optional<QJsonObject> parseBodyObject(const QHttpServerRequest& req,
                                                  QString* err = nullptr
                                                  ) {
    QJsonParseError pe{};
    auto doc = QJsonDocument::fromJson(req.body(), &pe);

    if (pe.error != QJsonParseError::NoError || !doc.isObject()) {
        if (err) {
            *err = pe.errorString();
        }

        return std::nullopt;
    }
    return doc.object();
}

inline bool requireFields(
    const QJsonObject& obj,
    std::initializer_list<const char*> keys,
    QString* missing = nullptr
    ) {

    for (auto k : keys) {
        if (!obj.contains(k)) {
            if (missing) {
                *missing = k;
                return false;
            }
        }
    }

    return true;
}

inline QJsonObject toJson(const Task& t) {
    return t.toJson();
}

inline Task fromJsonTaskStrict(const QJsonObject& obj, qint64 id = -1, QString* err = nullptr) {
    QString miss;

    if (!requireFields(obj, {"title","description","completed"}, &miss)) {
        if (err) {
            *err = QString("Missing field: %1").arg(miss);
        }
    }

    return Task::fromJson(obj, id);
}

inline Task patchedTask(const Task& original, const QJsonObject& patch) {
    Task t = original;
    if (patch.contains("title")) {
        t.title = patch.value("title").toString();
    }

    if (patch.contains("description")) {
        t.description = patch.value("description").toString();
    }

    if (patch.contains("completed")) {
        t.completed = patch.value("completed").toBool();
    }

    if (patch.contains("tags") && patch.value("tags").isArray()) {
        t.tags.clear();
        for (auto v : patch.value("tags").toArray()) {
            if (v.isObject()) {
                t.tags.append(Tag::fromJson(v.toObject()));
            }
        }
    }

    return t;
}

inline QJsonObject paginate(const QJsonArray& data, qsizetype page, qsizetype perPage) {
    const qsizetype N = data.size();

    if (perPage <= 0) {
        perPage = 10;
    }

    if (page <= 0) {
        page = 1;
    }

    const qsizetype totalPages = (N + perPage - 1) / perPage;
    const qsizetype start = (page - 1) * perPage;

    if (start >= N) {
        return QJsonObject{
            {"page", page},
            {"per_page", perPage},
            {"total", N},
            {"total_pages", totalPages},
            {"data", QJsonArray{} }
        };
    }

    QJsonArray slice;

    for (qsizetype i = 0; i < perPage && (start + i) < N; ++i) {
        slice.append(data.at(start + i));
    }

    return QJsonObject{
        {"page", page},
        {"per_page", perPage},
        {"total", N},
        {"total_pages", totalPages},
        {"data", slice}
    };
}

#endif // JSON_UTILS_HPP
