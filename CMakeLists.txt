cmake_minimum_required(VERSION 3.10.0)

macro(group_by_folder_internal BASEPATH SOURCE_FILES PREFIX)
  set(SOURCE_GROUP_DELIMITER "/")
  set(LAST_DIR "")
  set(FILES "")
  foreach(file ${SOURCE_FILES})
	file(RELATIVE_PATH relative_file ${BASEPATH} ${file})
	get_filename_component(dir "${relative_file}" PATH)
	if(NOT "${dir}" STREQUAL "${LAST_DIR}")
	  if(FILES)
		source_group("${PREFIX}${LAST_DIR}" FILES ${FILES})
	  endif(FILES)
	  set(FILES "")
	endif(NOT "${dir}" STREQUAL "${LAST_DIR}")
	set(FILES ${FILES} ${file})
	set(LAST_DIR "${dir}")
  endforeach(file)
  if(FILES)
	source_group("${PREFIX}${LAST_DIR}" FILES ${FILES})
  endif(FILES)
endmacro() 

macro(group_by_folder BASEPATH SOURCES)
  group_by_folder_internal(${BASEPATH} "${SOURCES}" "")
endmacro()

project(ECS VERSION 1.0.0 LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(ECS_BUILD_EXAMPLES "Build the ECS example programs" ON)
option(BUILD_SHARED_LIBS "Build shared libraries" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/lib)

if (NOT BUILD_SHARED_LIBS)
    set(ECS_STATIC 1)
endif()

add_subdirectory(src)

if (ECS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
