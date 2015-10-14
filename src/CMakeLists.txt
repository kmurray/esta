#
# Compiler flags come from parent
#

#
#
# Build files configuration
#
#

#Demo executable
#set(EXEC_NAME tatum_sta)

#Source files for the demo executable (not generated)
file(GLOB_RECURSE ETA_SOURCES *.cpp)
file(GLOB_RECURSE ETA_HEADERS *.hpp)

#Include directories
foreach(header ${ETA_HEADERS})
    get_filename_component(incl_dir ${header} DIRECTORY)
    list(APPEND ETA_INCLUDE_DIRS ${incl_dir})
endforeach()
#Remove duplicate include directories
list(REMOVE_DUPLICATES ETA_INCLUDE_DIRS)

#Define Executable
add_executable(eta
               ${ETA_SOURCES}
               ${ETA_HEADERS})

#Exectuable Includes
target_include_directories(eta PRIVATE
                           ${ETA_INCLUDE_DIRS})

#Executable links to the library
#target_link_libraries(eta blifparse tatum cudd_mtr cudd_st cudd_epd cudd_util cudd_obj cudd)
target_link_libraries(eta blifparse tatum 
                      #NOTE: the link order of the various CUDD sub-libraries is very important!
                      #      incorrect ordering will case linker errors!
                      cudd_obj cudd cudd_mtr cudd_st cudd_util cudd_epd )

