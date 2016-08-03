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
file(GLOB_RECURSE LIB_ESTA_SOURCES *.cpp)
file(GLOB_RECURSE LIB_ESTA_HEADERS *.hpp)

#Include directories
foreach(header ${LIB_ESTA_HEADERS})
    get_filename_component(incl_dir ${header} DIRECTORY)
    list(APPEND LIB_ESTA_INCLUDE_DIRS ${incl_dir})
endforeach()
list(REMOVE_DUPLICATES LIB_ESTA_INCLUDE_DIRS)

message(STATUS "LIB_ESTA Include Dirs: ${LIB_ESTA_INCLUDE_DIRS}")

#Define library
add_library(libesta STATIC ${LIB_ESTA_SOURCES} ${LIB_ESTA_HEADERS})
target_link_libraries(libesta
                      tatum
                      ${CUDD_LIBS})

#Library Includes
target_include_directories(libesta PUBLIC ${LIB_ESTA_INCLUDE_DIRS})
