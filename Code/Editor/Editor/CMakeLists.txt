ez_cmake_init()

ez_requires_editor()

ez_requires_windows()

# Get the name of this folder as the project name
get_filename_component(PROJECT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME_WE)

ez_create_target(APPLICATION ${PROJECT_NAME})

ez_make_winmain_executable(${PROJECT_NAME})

ez_link_target_qt(TARGET ${PROJECT_NAME} COMPONENTS Core Gui Widgets Network Svg)

target_link_libraries(${PROJECT_NAME}
  PRIVATE
  EditorFramework
  EditorEngineProcessFramework
)

add_dependencies(${PROJECT_NAME}
  EditorProcessor
  EditorEngineProcess
  Player
)

# set all external projects as runtime dependencies of this application
get_property(EXTERNAL_PROJECTS GLOBAL PROPERTY "EXTERNAL_PROJECTS")
if(EXTERNAL_PROJECTS)
  message(STATUS "External Projects: ${EXTERNAL_PROJECTS}")
  add_dependencies(${PROJECT_NAME} ${EXTERNAL_PROJECTS})
endif()

set(EDITOR_DATA_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}/../../../Data/Tools/ezEditor")
file(GLOB_RECURSE EDITOR_DATA "${CMAKE_CURRENT_SOURCE_DIR}/../../../Data/Tools/ezEditor/*")

source_group(TREE ${EDITOR_DATA_FOLDER} PREFIX "Editor Data" FILES ${EDITOR_DATA})
target_sources(${PROJECT_NAME} PRIVATE "${EDITOR_DATA}")

set_source_files_properties(${EDITOR_DATA} PROPERTIES HEADER_FILE_ONLY TRUE)
