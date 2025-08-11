#ifndef TASKPATCH_HPP
#define TASKPATCH_HPP

#include "task.hpp"
#include <QJsonObject>
#include <QJsonArray>

inline void applyTaskPatch(Task& t, const QJsonObject& obj) {
    if (obj.contains("title")) {
        t.title = obj.value("title").toString();
    }

    if (obj.contains("description")) {
        t.description = obj.value("description").toString();
    }

    if (obj.contains("completed")) {
        t.completed = obj.value("completed").toBool();
    }

    if (obj.contains("tags") && obj.value("tags").isArray()) {
        QList<Tag> newTags;
        const auto arr = obj.value("tags").toArray();
        for (const auto& v : arr) {
            if (!v.isObject()) continue;
            newTags.append(Tag::fromJson(v.toObject()));
        }
        t.tags = std::move(newTags);
    }
}

#endif // TASKPATCH_HPP
