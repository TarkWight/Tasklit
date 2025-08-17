#ifndef TASK_HPP
#define TASK_HPP

#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QUuid>
#include <QVector>
#include <optional>

#include "Tag.hpp"

struct Task {
    QUuid id;
    QString title;
    QString description;
    bool isCompleted = false;

    QVector<QUuid> tags;

    std::optional<QVector<Tag>> tagsExpanded;

    QJsonObject toJson(bool includeExpanded = false) const {
        QJsonArray tagIdsArray;
        for (const QUuid &tagId : tags) {
            tagIdsArray.append(tagId.toString(QUuid::WithoutBraces));
        }

        QJsonObject json{{"id", id.toString(QUuid::WithoutBraces)},
                         {"title", title},
                         {"description", description},
                         {"isCompleted", isCompleted},
                         {"tags", tagIdsArray}};

        if (includeExpanded && tagsExpanded && !tagsExpanded->isEmpty()) {
            QJsonArray expanded;
            for (const Tag &tag : *tagsExpanded) {
                expanded.append(tag.toJson());
            }
            json.insert("tagsExpanded", expanded);
        }

        return json;
    }

    static Task fromJson(const QJsonObject &jsonObject, const QUuid &forcedId = QUuid()) {
        Task task;

        task.id = forcedId.isNull()
                      ? QUuid::fromString(jsonObject.value("id").toString())
                      : forcedId;

        task.title = jsonObject.value("title").toString();
        task.description = jsonObject.value("description").toString();
        task.isCompleted = jsonObject.value("isCompleted").toBool(false);

        if (jsonObject.contains("tags") && jsonObject.value("tags").isArray()) {
            const QJsonArray tagsArray = jsonObject.value("tags").toArray();
            task.tags.clear();
            task.tags.reserve(static_cast<int>(tagsArray.size()));

            for (const QJsonValue &value : tagsArray) {
                QUuid parsedId;
                if (value.isString()) {
                    parsedId = QUuid::fromString(value.toString());
                } else if (value.isObject()) {
                    parsedId =
                        QUuid::fromString(value.toObject().value("id").toString());
                }

                if (!parsedId.isNull()) {
                    task.tags.push_back(parsedId);
                }
            }
        }

        task.tagsExpanded.reset();

        return task;
    }
};

#endif // TASK_HPP
