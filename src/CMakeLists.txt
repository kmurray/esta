#
# Compiler flags come from parent
#

#
#
# Build files configuration
#
#

#Source files
file(GLOB_RECURSE ETA_SOURCES */*.cpp)
file(GLOB_RECURSE ETA_HEADERS */*.hpp)
file(GLOB ETA_MAIN main.cpp)

#Include directories
foreach(header ${ETA_HEADERS})
    get_filename_component(incl_dir ${header} DIRECTORY)
    list(APPEND ETA_INCLUDE_DIRS ${incl_dir})
endforeach()
#Remove duplicate include directories
list(REMOVE_DUPLICATES ETA_INCLUDE_DIRS)

#Define library
add_library(libeta STATIC ${ETA_SOURCES} ${ETA_HEADERS})
target_link_libraries(libeta
                      tatum
                      blifparse
                      ${CUDD_LIBS})

#Library Includes
target_include_directories(libeta PUBLIC
                           ${ETA_INCLUDE_DIRS})


#Define Executable
add_executable(eta
               ${ETA_MAIN})

#Executable links to the library
#target_link_libraries(eta blifparse tatum cudd_mtr cudd_st cudd_epd cudd_util cudd_obj cudd)
target_link_libraries(eta 
                      cpp-argparse 
                      libeta)

