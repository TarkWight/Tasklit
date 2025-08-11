find_package(Qt6 REQUIRED COMPONENTS Core Gui)

add_library(utils
  json_utils.hpp
)

target_link_libraries(utils PUBLIC
  Qt6::Core
  Qt6::Gui
)

target_include_directories(utils PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
