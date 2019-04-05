CMAKE_MINIMUM_REQUIRED (VERSION 3.2)
PROJECT (Test C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_MODULE_PATH
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake.modules
)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed
set(CMAKE_AUTOMOC ON)
# Create code from a list of Qt designer ui files
set(CMAKE_AUTOUIC ON)

SET(CMAKE_PREFIX_PATH "$ENV{CMAKE_PREFIX_PATH};${CMAKE_PREFIX_PATH}")

# Find the QtWidgets library
find_package(Qt5Core CONFIG REQUIRED)

if ($ENV{BOOST_INCLUDEDIR})
	set(BOOST_INCLUDEDIR $ENV{BOOST_INCLUDEDIR})
endif()
if ($ENV{BOOST_LIBRARYDIR})
	set(BOOST_LIBRARYDIR $ENV{BOOST_LIBRARYDIR})
endif()
find_package(Boost 1.66 REQUIRED)

get_target_property(QtCore_location Qt5::Core LOCATION)

STRING(REPLACE ":" ";" default_includes "$ENV{C_INCLUDE_PATH}")
STRING(REPLACE ":" ";" default_libs "$ENV{LIBRARY_PATH}")

INCLUDE_DIRECTORIES(
	inc
	${Qt5Core_INCLUDE_DIRS}
	${default_includes}
	${Boost_INCLUDE_DIRS}
)

LINK_DIRECTORIES(
	${default_libs}
	${Boost_LIBRARY_DIRS}
)

foreach(DIR src inc)
	MESSAGE(STATUS "processing ${DIR}")
	file(GLOB_RECURSE TMP RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
		${DIR}/*.cpp
		${DIR}/*.h
		${DIR}/*.hpp
		${DIR}/*.c
		${DIR}/*.cxx
		${DIR}/*.cc
		)
	list(APPEND Common-files ${TMP})
    list(APPEND FILES_TO_FORMAT ${TMP})
endforeach()

ADD_EXECUTABLE(Main ${Common-files} main.cxx)

SET(DEP_3rdParty Qt5::Core liblmdb.a pthread ${Boost_LIBRARIES})

SET_TARGET_PROPERTIES(Main PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries(Main ${DEP_3rdParty})

set(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS_RELEASE} -fprofile-arcs -ftest-coverage")

# set up a mapping so that the Release configuration for the Qt imported target is
# used in the COVERAGE CMake configuration.
set_target_properties(Qt5::Core PROPERTIES MAP_IMPORTED_CONFIG_COVERAGE "RELEASE")

foreach(DIR bench)
	MESSAGE(STATUS "processing ${DIR}")
	file(GLOB_RECURSE TMP RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
		${DIR}/*.cpp
		${DIR}/*.h
		${DIR}/*.hpp
		${DIR}/*.c
		${DIR}/*.cxx
		${DIR}/*.cc
		)
	list(APPEND Bench-files ${TMP})
    list(APPEND FILES_TO_FORMAT ${TMP})
endforeach()

ADD_EXECUTABLE(Bench ${Bench-files} ${Common-files})

target_link_libraries(Bench ${DEP_3rdParty} benchmark pthread)

foreach(DIR tests)
	MESSAGE(STATUS "processing ${DIR}")
	file(GLOB_RECURSE TMP RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
		${DIR}/*.cpp
		${DIR}/*.h
		${DIR}/*.hpp
		${DIR}/*.c
		${DIR}/*.cxx
		${DIR}/*.cc
		)
	list(APPEND Test-files ${TMP})
    list(APPEND FILES_TO_FORMAT ${TMP})
endforeach()

ADD_EXECUTABLE(Test ${Test-files} ${Common-files})

target_link_libraries(Test ${DEP_3rdParty} gtest gmock pthread)

add_custom_target(
clang-format-project-files
WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
DEPENDS ${FILES_TO_FORMAT}
COMMAND clang-format -i -style=file ${FILES_TO_FORMAT}
)

option(BUILD_DOC "Build documentation" ON)

# check if Doxygen is installed
find_package(Doxygen)
if (DOXYGEN_FOUND)
    # set input and output files
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/doxygen/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    # request to configure the file
    configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
    message("Doxygen build started")

    # note the option ALL which allows to build the docs together with the application
    add_custom_target( doc_doxygen ALL
        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM )
else (DOXYGEN_FOUND)
  message("Doxygen need to be installed to generate the doxygen documentation")
endif (DOXYGEN_FOUND)
