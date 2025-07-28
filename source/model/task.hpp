#ifndef TASK_HPP
#define TASK_HPP

#include <QString>
#include <QJsonObject>
#include <QJsonArray>

#include <cstdint>

#include "tag.hpp"

struct Task {
    int64_t id;
    QString title;
    QString description;
    bool completed = false;
    QList<Tag> tags;

    QJsonObject toJson() const {
        QJsonArray tagsArray;
        for (const auto &tag : tags) {
            tagsArray.append(tag.toJson());
        }

        return {
            { "id", id },
            { "title", title },
            { "description", description },
            { "completed", completed },
            { "tags", tagsArray }
        };
    }

    static Task fromJson(const QJsonObject &obj, int64_t id = -1) {
        Task task;
        task.id = id;
        task.title = obj["title"].toString();
        task.description = obj["description"].toString();
        task.completed = obj["completed"].toBool();

        QJsonArray tagArray = obj["tags"].toArray();
        for (int i = 0; i < tagArray.size(); ++i) {
            const auto &t = tagArray.at(i);
            if (t.isObject()) {
                task.tags.append(Tag::fromJson(t.toObject()));
            }
        }

        return task;
    }
};

#endif // TASK_HPP
