add_executable(Tasklit
  main.cpp
)

add_subdirectory(model)
add_subdirectory(storage)
add_subdirectory(service)
add_subdirectory(http)

target_link_libraries(Tasklit
  Qt6::Core
  Qt6::Network
  Qt6::Sql
  Qt6::HttpServer
  Qt6::Concurrent
  model
  storage
  service
  http
)

target_include_directories(Tasklit PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)
