cmake_minimum_required(VERSION 3.16)

project(Tasklit LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOUIC OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC OFF)

find_package(Qt6 REQUIRED COMPONENTS Core Network Sql HttpServer Concurrent)

add_executable(Tasklit
  src/main.cpp
)

target_link_libraries(Tasklit
  Qt6::Core
  Qt6::Network
  Qt6::Sql
  Qt6::HttpServer
  Qt6::Concurrent
)

target_include_directories(Tasklit PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

include(GNUInstallDirs)
install(TARGETS Tasklit
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
