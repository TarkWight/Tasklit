# Tasklit
Tasklit — Lightweight C++ REST API server for task management, built on Qt 6 and SQLite.

---

## Features
- REST API for managing tasks and tags
- JSON-based request/response
- UUID for entity identification
- SQLite storage backend
- Modular architecture (`http`, `service`, `storage`, `model`, `utils`)
- Postman collection for testing

---

## Build & Run
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
./Tasklit
```

Default server address: `http://localhost:8080` (setup source/main.cpp

---

## Models

### Task
```json
{
  "id": "uuid",
  "title": "string",
  "description": "string",
  "isCompleted": true,
  "tags": ["uuid", "uuid"]
}
```

### Tag
```json
{
  "id": "uuid",
  "name": "string"
}
```

---

## API Endpoints

### Tasks

#### Get all tasks
```
GET /tasks
```
Response: list of tasks

#### Create task
```
POST /task/create
Content-Type: application/json

{
  "title": "First task",
  "description": "Check server",
  "tags": [],
  "isCompleted": true
}
```

#### Get task by ID
```
GET /task?id=<uuid>
```

#### Update part of task
```
PATCH /task?id=<uuid>
Content-Type: application/json

{
  "title": "Updated title",
  "description": "Updated description",
  "isCompleted": true,
  "tags": []
}
```

#### Delete task
```
DELETE /task?id=<uuid>
```

#### Delete all tasks
```
DELETE /tasks
```

---

### Tags

#### Get all tags
```
GET /tags
```

#### Create new tag
```
POST /tag/create
Content-Type: application/json

{
  "name": "qt@"
}
```

*(Planned)*: Delete tag by ID, Delete all tags.

---

## Project Structure
```
source/
 ├── main
 ├── http/       # Routers
 ├── model/      # Data models
 ├── service/    # Business logic
 ├── storage/    # Database layer
 ├── utils/      # Helpers (Logger, JsonUtils, TaskPatch, ErrorHandler)
```

---

## Postman Collection
A ready-to-use Postman collection is available in `helpers/Qt Tasks API.postman_collection.json`.

You can test endpoints by importing this file into Postman.

---

## Roadmap
- [ ] Delete tag by ID / all tags
- [ ] Pagination for tasks
- [ ] Search & filtering
- [ ] Add created/updated timestamps
- [ ] Improve logging & error handling
- [ ] Add tests (unit, integration)
