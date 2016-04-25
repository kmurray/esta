#
# Compiler flags come from parent
#

#
#
# Build files configuration
#
#

#
# Library Source files
#
file(GLOB_RECURSE LIB_ETA_SOURCES libeta/*.cpp)
file(GLOB_RECURSE LIB_ETA_HEADERS libeta/*.hpp)

#Include directories
foreach(header ${LIB_ETA_HEADERS})
    get_filename_component(incl_dir ${header} DIRECTORY)
    list(APPEND LIB_ETA_INCLUDE_DIRS ${incl_dir})
endforeach()
#Remove duplicate include directories
list(REMOVE_DUPLICATES LIB_ETA_INCLUDE_DIRS)

message(STATUS "ETA_LIB Includ Dirs: ${LIB_ETA_INCLUDE_DIRS}")
#
# Executable Source files
#
file(GLOB_RECURSE EXEC_ETA_SOURCES main/*.cpp)
file(GLOB_RECURSE EXEC_ETA_HEADERS main/*.hpp)

#Include directories
foreach(header ${EXEC_ETA_HEADERS})
    get_filename_component(incl_dir ${header} DIRECTORY)
    list(APPEND EXEC_ETA_INCLUDE_DIRS ${incl_dir})
endforeach()
#Remove duplicate include directories
list(REMOVE_DUPLICATES EXEC_ETA_INCLUDE_DIRS)

message(STATUS "ETA_EXEC Includ Dirs: ${EXEC_ETA_INCLUDE_DIRS}")

#Define library
add_library(libeta STATIC ${LIB_ETA_SOURCES} ${LIB_ETA_HEADERS})
target_link_libraries(libeta
                      tatum
                      ${CUDD_LIBS})

#Library Includes
target_include_directories(libeta PUBLIC ${LIB_ETA_INCLUDE_DIRS})


#Define Executable
add_executable(esta ${EXEC_ETA_SOURCES} ${EXEC_ETA_HEADERS})

#Executable links to the library
target_link_libraries(esta 
                      cpp-argparse 
                      blifparse
                      sdfparse
                      libyaml-cpp.a #TODO: find this properly...
                      libeta)

