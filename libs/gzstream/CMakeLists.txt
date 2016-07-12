#
# Compiler flags come from parent
#

#Needs ZLIB
find_package(ZLIB REQUIRED)

#Source files for the library
file(GLOB_RECURSE GZSTREAM_SOURCES *.C)
file(GLOB_RECURSE GZSTREAM_HEADERS *.h)

#Include directories
foreach(header ${GZSTREAM_HEADERS})
    get_filename_component(incl_dir ${header} DIRECTORY)
    list(APPEND GZSTREAM_INCLUDE_DIRS ${incl_dir})
endforeach()
#Remove duplicate include directories
list(REMOVE_DUPLICATES GZSTREAM_INCLUDE_DIRS)


#
#
# Define the actual build targets
#
#

#Define the library
add_library(gzstream STATIC ${GZSTREAM_SOURCES} ${GZSTREAM_HEADERS})

#Export library headers
target_include_directories(gzstream PUBLIC ${GZSTREAM_INCLUDE_DIRS})
target_link_libraries(gzstream ${ZLIB_LIBRARIES})

