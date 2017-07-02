cmake_minimum_required(VERSION 2.8.11)

# Name of the project
project(binmerge)

# Add subprojects
add_subdirectory(docopt.cpp)

# Enable C++14 features for g++
if(${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14")
endif()

# Specify output locations for executables
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/bin)

# Collect source and header files
set(HEADERS )

set(SOURCES
  binmerge.cpp
)

# Create the executable
add_executable(binmerge ${HEADERS} ${SOURCES})

# Link against docopt library
target_link_libraries(binmerge docopt_s)
