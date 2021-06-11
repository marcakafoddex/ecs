file(GLOB SOURCES 
	*.cpp
	*.hh
	*.h)

link_libraries(ecs)
add_executable(ecs-tests ${SOURCES})

target_include_directories(ecs-tests PRIVATE
                           "${ECS_SOURCE_DIR}/examples"
                           "${ECS_SOURCE_DIR}/tests")

group_by_folder(${CMAKE_CURRENT_SOURCE_DIR} "${SOURCES}")
