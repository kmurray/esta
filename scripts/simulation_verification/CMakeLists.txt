#
# Executable Source files
#
file(GLOB_RECURSE EXEC_VCD_EXTRACT_SOURCES vcd_extract/*.cpp)
file(GLOB_RECURSE EXEC_VCD_EXTRACT_HEADERS vcd_extract/*.hpp)

#Include directories
set(EXEC_VCD_EXTRACT_INCLUDE_DIRS "")
foreach(header ${EXEC_VCD_EXTRACT_HEADERS})
    get_filename_component(incl_dir ${header} DIRECTORY)
    list(APPEND EXEC_VCD_EXTRACT_INCLUDE_DIRS ${incl_dir})
endforeach()
#Remove duplicate include directories
list(REMOVE_DUPLICATES EXEC_VCD_EXTRACT_INCLUDE_DIRS)

#Define library
add_executable(vcd_extract ${EXEC_VCD_EXTRACT_SOURCES} ${EXEC_VCD_EXTRACT_HEADERS})
target_link_libraries(vcd_extract
                      vcdparse)
