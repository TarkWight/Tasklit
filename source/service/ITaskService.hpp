#ifndef TASKLIT_SERVICE_ITASKSERVICE_HPP
#define TASKLIT_SERVICE_ITASKSERVICE_HPP

#include <QUuid>
#include <optional>
#include <vector>

#include "Task.hpp"
#include "Tag.hpp"

class ITaskService {
public:
    virtual ~ITaskService() = default;

    virtual std::vector<Task> getAllTasks() const = 0;
    virtual std::optional<Task> getTaskById(const QUuid &taskId) const = 0;

    virtual QUuid addTask(const Task &task) = 0;
    virtual bool updateTask(const QUuid &taskId, const Task &task) = 0;
    virtual bool deleteTask(const QUuid &taskId) = 0;
    virtual bool deleteAll() = 0;

    virtual std::vector<Tag> getAllTags() const = 0;
    virtual QUuid addTag(const Tag &tag) = 0;
};

#endif // TASKLIT_SERVICE_ITASKSERVICE_HPP
