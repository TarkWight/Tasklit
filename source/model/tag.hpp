#ifndef TAG_HPP
#define TAG_HPP

#include <QJsonObject>
#include <QString>
#include <QUuid>

struct Tag {
    QUuid id;
    QString name;

    QJsonObject toJson() const {
        return QJsonObject{{"id", id.toString(QUuid::WithoutBraces)},
                           {"name", name}};
    }

    static Tag fromJson(const QJsonObject &jsonObject, const QUuid &forcedId = QUuid()) {
        Tag tag;
        tag.id = forcedId.isNull()
                     ? QUuid::fromString(jsonObject.value("id").toString())
                     : forcedId;
        tag.name = jsonObject.value("name").toString();

        return tag;
    }
};

#endif // TAG_HPP
