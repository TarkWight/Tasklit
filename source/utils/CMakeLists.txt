find_package(Qt6 REQUIRED COMPONENTS Core Gui)

add_library(utils
  JsonUtils.hpp
  Logger.hpp
  Logger.cpp
  TaskPatch.hpp
  ErrorHandler.hpp
)

target_link_libraries(utils PUBLIC
  Qt6::HttpServer
  Qt6::Core
  Qt6::Gui
  model
)

target_include_directories(utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
