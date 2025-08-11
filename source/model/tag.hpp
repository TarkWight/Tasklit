#ifndef TAG_HPP
#define TAG_HPP

#include <QString>
#include <QJsonObject>

#include <cstdint>

struct Tag {
    int64_t id;
    QString name;

    QJsonObject toJson() const {
        return {
            { "id", id },
            { "name", name }
        };
    }

    static Tag fromJson(const QJsonObject &obj, int64_t id = -1) {
        return {
            .id = id,
            .name = obj["name"].toString()
        };
    }
};

#endif // TAG_HPP
