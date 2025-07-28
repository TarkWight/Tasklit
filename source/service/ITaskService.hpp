#ifndef TASKLIT_SERVICE_ITASKSERVICE_HPP
#define TASKLIT_SERVICE_ITASKSERVICE_HPP

#include <vector>
#include <optional>

#include "task.hpp"

class ITaskService {
public:
    virtual ~ITaskService() = default;

    virtual std::vector<Task> getAllTasks() const = 0;
    virtual std::optional<Task> getTaskById(qint64 id) const = 0;

    virtual qint64 addTask(const Task& task) = 0;
    virtual bool updateTask(qint64 id, const Task& task) = 0;
    virtual bool deleteTask(qint64 id) = 0;
    virtual bool deleteAll() = 0;
};

#endif // TASKLIT_SERVICE_ITASKSERVICE_HPP
