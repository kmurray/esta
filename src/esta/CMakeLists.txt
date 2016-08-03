#
# Executable Source files
#
file(GLOB_RECURSE ESTA_SOURCES *.cpp)
file(GLOB_RECURSE ESTA_HEADERS *.hpp)

#Define Executable
add_executable(esta ${ESTA_SOURCES} ${ESTA_HEADERS})

#Executable links to the library
target_link_libraries(esta 
                      cpp-argparse 
                      blifparse
                      sdfparse
                      libesta)
