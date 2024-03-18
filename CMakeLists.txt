cmake_minimum_required(VERSION 3.16...3.26)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_qt(COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt::Core Qt::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME} PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
                                  -Wno-comma>)
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON
               AUTOUIC ON
               AUTORCC ON
               AUTOUIC_SEARCH_PATHS src/forms)
endif()

# C
target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/plugin-main.c)
# C++/UI
target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/roi-editor.cpp src/roi-editor.hpp)
target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/forms/roi-editor.ui)

# Out of tree compile
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE BUILD_STANDALONE)

# utils stolen from OBS UI
target_sources(
  ${CMAKE_PROJECT_NAME} PRIVATE src/external/display-helpers.hpp src/external/qt-display.hpp
                                src/external/qt-display.cpp src/external/qt-wrappers.hpp src/external/qt-wrappers.cpp)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
