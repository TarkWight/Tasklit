#ifndef TASKPATCH_HPP
#define TASKPATCH_HPP

#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

#include "Task.hpp"

inline QVector<QUuid> parseTagIdsFromJsonArray(const QJsonArray &arr) {
    QVector<QUuid> out;
    out.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        QUuid id;
        if (v.isString()) {
            id = QUuid::fromString(v.toString());
        } else if (v.isObject()) {
            id = QUuid::fromString(v.toObject().value("id").toString());
        }
        if (!id.isNull()) {
            out.push_back(id);
        }
    }
    return out;
}

// Применяем частичный патч к Task.
// Поддерживаем ключи:
//  - "title": string
//  - "description": string
//  - "isCompleted": bool
//  - "tags": array<string|{id:string}> | null  → полная замена набора тегов
inline void applyTaskPatch(Task &task, const QJsonObject &obj) {
    if (obj.contains("title")) {
        task.title = obj.value("title").toString();
    }

    if (obj.contains("description")) {
        task.description = obj.value("description").toString();
    }

    if (obj.contains("isCompleted")) {
        task.isCompleted = obj.value("isCompleted").toBool(task.isCompleted);
    }

    if (obj.contains("tags")) {
        const QJsonValue tagsVal = obj.value("tags");
        if (tagsVal.isArray()) {
            task.tags = parseTagIdsFromJsonArray(tagsVal.toArray());
        } else if (tagsVal.isNull()) {
            task.tags.clear();
        } else {
            // игнорируем некорректный формат, намеренно не меняем текущие теги
        }

        task.tagsExpanded.reset();
    }
}

#endif // TASKPATCH_HPP
