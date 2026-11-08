cmake_minimum_required(VERSION 3.10...3.28)

project(argparser VERSION 0.1.0 LANGUAGES CXX)

include(GNUInstallDirs)

# exe template begin
add_executable(app src/main.cpp src/argparser.cpp)

set_target_properties(app PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)

target_include_directories(app PRIVATE ${CMAKE_SOURCE_DIR}/include)
# exe template end

# lib template begin
add_library(argparser SHARED src/argparser.cpp)

target_include_directories(argparser PRIVATE ${CMAKE_SOURCE_DIR}/include)

set_target_properties(argparser PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON)

install(TARGETS argparser
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
# lib template end
