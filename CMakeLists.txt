cmake_minimum_required(VERSION 3.0.2)
project(nlib)

add_compile_options(-std=c++14)

find_package(catkin REQUIRED COMPONENTS
	roscpp)

catkin_package(
	CATKIN_DEPENDS roscpp nav_msgs
	DEPENDS boost
	INCLUDE_DIRS include
	LIBRARIES nlib
)

include_directories(
	include
	${catkin_INCLUDE_DIRS}
)

add_executable (example
	example/example_node.cpp example/example_modflow.cpp)
target_link_libraries (example ${catkin_LIBRARIES})

add_library (nlib src/nlib.cpp)

install(TARGETS nlib
	   ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
	   LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
	   RUNTIME DESTINATION ${CATKIN_GLOBAL_BIN_DESTINATION})

install (
	DIRECTORY include/nlib/
	DESTINATION ${CATKIN_PACKAGE_INCLUDE_DESTINATION}
	PATTERN ".h")
