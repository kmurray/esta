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

#Define Executable
add_executable(eta
               ${ETA_SOURCES}
               ${ETA_HEADERS})

#Exectuable Includes
target_include_directories(eta PRIVATE
                           ${ETA_INCLUDE_DIRS})

#Executable links to the library
target_link_libraries(eta blifparse tatum)

