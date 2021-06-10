file(GLOB SOURCES 
	*.cpp
	*.hh
	*/*.hh
	*/*.cpp)

link_libraries(ecs)
add_executable(ecs-example ${SOURCES})

target_include_directories(ecs-example PRIVATE
                           "${ECS_SOURCE_DIR}/examples")

group_by_folder(${CMAKE_CURRENT_SOURCE_DIR} "${SOURCES}")
