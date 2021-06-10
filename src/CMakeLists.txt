add_library(ecs Ecs.cpp ../include/Ecs.hh ../include/Advanced.hh ../include/Call.hh)
set_target_properties(ecs PROPERTIES
                      OUTPUT_NAME ecs
                      VERSION ${ECS_VERSION_MAJOR}.${ECS_VERSION_MINOR}
                      SOVERSION ${ECS_VERSION_MAJOR}
                      POSITION_INDEPENDENT_CODE ON
                      FOLDER "ecs")

if (BUILD_SHARED_LIBS)
	target_compile_definitions(ecs PRIVATE ECS_BUILD_DLL)
endif()

target_include_directories(ecs PUBLIC
                           "$<BUILD_INTERFACE:${ECS_SOURCE_DIR}/include>"
                           "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>")
target_include_directories(ecs PRIVATE
                           "${ECS_SOURCE_DIR}/src"
                           "${ECS_BINARY_DIR}/src"
                           ${ecs_INCLUDE_DIRS})
