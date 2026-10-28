cmake_minimum_required(VERSION 3.10)
project(WorkshopTests)

include(ExternalProject)
set(googletestArgs -DCMAKE_INSTALL_PREFIX=${CMAKE_SOURCE_DIR}/lib/googletest/install)
ExternalProject_add(googletest
     GIT_REPOSITORY https://github.com/google/googletest.git
     GIT_TAG main
     CMAKE_ARGS ${googletestArgs}
     SOURCE_DIR ${CMAKE_SOURCE_DIR}/lib/googletest/src
     BINARY_DIR ${CMAKE_SOURCE_DIR}/lib/googletest/build
     INSTALL_DIR ${CMAKE_SOURCE_DIR}/lib/googletest/install
     UPDATE_COMMAND ""
)

include_directories(
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/lib/googletest/install/include
)

link_directories(
	${CMAKE_SOURCE_DIR}/lib/googletest/install/lib
	${CMAKE_SOURCE_DIR}/lib/googletest/install/lib64
)

set(SRC_FILES
	main.cpp
	date.cpp
)

set(TEST_FILES
##### EDIT ME!!! #####################
	date.cpp
######################################
	date_unittest.cpp
)

add_executable (date ${SRC_FILES})
add_executable (date_unittests ${TEST_FILES})

target_link_libraries(date_unittests gtest_main gtest)
