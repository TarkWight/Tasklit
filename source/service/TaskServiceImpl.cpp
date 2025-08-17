#include "Logger.hpp"
#include "TaskServiceImpl.hpp"

#include <QSet>

TaskServiceImpl::TaskServiceImpl(std::shared_ptr<IStorage> storage)
    : m_storage(std::move(storage)) {}

// ───────────────────────────────────────────────
// Tasks
// ───────────────────────────────────────────────

std::vector<Task> TaskServiceImpl::getAllTasks() const {
    auto tasks = m_storage->getAllTasks();
    qInfo(appCore) << "[Server] Retrieved" << tasks.size() << "tasks";
    return tasks;
}

std::optional<Task> TaskServiceImpl::getTaskById(const QUuid &taskId) const {
    if (taskId.isNull()) {
        qWarning(appCore) << "[Server] getTaskById called with null id";
        return std::nullopt;
    }
    auto task = m_storage->getTaskById(taskId);
    if (task) {
        qInfo(appCore) << "[Server] Task found:" << task->title;
    } else {
        qWarning(appCore) << "[Server] Task with id" << taskId.toString()
        << "not found";
    }
    return task;
}

QUuid TaskServiceImpl::addTask(const Task &task) {
    if (task.title.trimmed().isEmpty()) {
        qWarning(appCore) << "[Server] Attempt to add task with empty title";
        return QUuid();
    }

    auto toStore = task;
    if (toStore.id.isNull()) {
        toStore.id = QUuid::createUuid();
    }

    {
        QVector<QUuid> unique;
        QSet<QUuid> seen;
        for (const QUuid &tid : std::as_const(toStore.tags)) {
            if (!tid.isNull() && !seen.contains(tid)) {
                unique.push_back(tid);
                seen.insert(tid);
            }
        }
        toStore.tags = std::move(unique);
    }

    const QUuid storedId = m_storage->addTask(toStore);
    if (!storedId.isNull()) {
        qInfo(appCore) << "[Server] Task added:" << toStore.title
                       << "(id=" << storedId.toString() << ")";
    } else {
        qCritical(appCore) << "[Server] Failed to add task:" << toStore.title;
    }

    return storedId;
}

bool TaskServiceImpl::updateTask(const QUuid &taskId, const Task &task) {
    if (taskId.isNull()) {
        qWarning(appCore) << "[Server] Attempt to update task with null id";
        return false;
    }

    Task toSave = task;
    toSave.id = taskId;

    QVector<QUuid> unique;
    QSet<QUuid> seen;
    for (const QUuid &tid : std::as_const(toSave.tags)) {
        if (!tid.isNull() && !seen.contains(tid)) {
            unique.push_back(tid);
            seen.insert(tid);
        }
    }
    toSave.tags = std::move(unique);

    bool ok = m_storage->updateTask(taskId, toSave);
    if (ok) {
        qInfo(appCore) << "[Server] Task updated:" << toSave.title
                       << "(id=" << taskId.toString() << ")";
    } else {
        qCritical(appCore) << "[Server] Failed to update task (id="
                           << taskId.toString() << ")";
    }

    return ok;
}

bool TaskServiceImpl::deleteTask(const QUuid &taskId) {
    if (taskId.isNull()) {
        qWarning(appCore) << "[Server] Attempt to delete task with null id";
        return false;
    }

    bool ok = m_storage->deleteTask(taskId);
    if (ok) {
        qInfo(appCore) << "[Server] Task deleted (id=" << taskId.toString()
        << ")";
    } else {
        qCritical(appCore) << "[Server] Failed to delete task (id="
                           << taskId.toString() << ")";
    }

    return ok;
}

bool TaskServiceImpl::deleteAll() {
    bool ok = m_storage->deleteAll();
    if (ok) {
        qInfo(appCore) << "[Server] All tasks deleted";
    } else {
        qCritical(appCore) << "[Server] Failed to delete all tasks";
    }

    return ok;
}

std::vector<Tag> TaskServiceImpl::getAllTags() const {
    return m_storage->getAllTags();
}

QUuid TaskServiceImpl::addTag(const Tag &tag) {
    if (tag.name.trimmed().isEmpty()) {
        return QUuid();
    }

    Tag toStore = tag;
    if (toStore.id.isNull()) {
        toStore.id = QUuid::createUuid();
    }

    const QUuid storedId = m_storage->addTag(toStore);
    return storedId;
}
