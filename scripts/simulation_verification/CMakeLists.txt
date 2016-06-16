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

#foreach(cpp_file ${EXEC_VCD_EXTRACT_SOURCES})
    #set_source_files_properties(${cpp_file} PROPERTIES COMPILE_FLAGS -g)
    #set_source_files_properties(${cpp_file} PROPERTIES COMPILE_FLAGS -O0)
#endforeach()

#Define library
add_executable(vcd_extract ${EXEC_VCD_EXTRACT_SOURCES} ${EXEC_VCD_EXTRACT_HEADERS})
target_link_libraries(vcd_extract
                      vcdparse)

#
# Executable Source files
#
file(GLOB_RECURSE EXEC_VCD_EXTRACT_PIPE_SOURCES vcd_extract_pipe/*.cpp)
file(GLOB_RECURSE EXEC_VCD_EXTRACT_PIPE_HEADERS vcd_extract_pipe/*.hpp)

#Include directories
set(EXEC_VCD_EXTRACT_PIPE_INCLUDE_DIRS "")
foreach(header ${EXEC_VCD_EXTRACT_PIPE_HEADERS})
    get_filename_component(incl_dir ${header} DIRECTORY)
    list(APPEND EXEC_VCD_EXTRACT_PIPE_INCLUDE_DIRS ${incl_dir})
endforeach()
#Remove duplicate include directories
list(REMOVE_DUPLICATES EXEC_VCD_EXTRACT_PIPE_INCLUDE_DIRS)

#foreach(cpp_file ${EXEC_VCD_EXTRACT_PIPE_SOURCES})
    #set_source_files_properties(${cpp_file} PROPERTIES COMPILE_FLAGS -g)
    #set_source_files_properties(${cpp_file} PROPERTIES COMPILE_FLAGS -O0)
#endforeach()

#Define library
#add_executable(vcd_extract_pipe ${EXEC_VCD_EXTRACT_PIPE_SOURCES} ${EXEC_VCD_EXTRACT_PIPE_HEADERS})

#
# Executable Source files
#
file(GLOB_RECURSE EXEC_VCD_SPLIT_SOURCES vcd_split/*.cpp)
file(GLOB_RECURSE EXEC_VCD_SPLIT_HEADERS vcd_split/*.hpp)

#Include directories
set(EXEC_VCD_SPLIT_INCLUDE_DIRS "")
foreach(header ${EXEC_VCD_SPLIT_HEADERS})
    get_filename_component(incl_dir ${header} DIRECTORY)
    list(APPEND EXEC_VCD_SPLIT_INCLUDE_DIRS ${incl_dir})
endforeach()
#Remove duplicate include directories
list(REMOVE_DUPLICATES EXEC_VCD_SPLIT_INCLUDE_DIRS)

#foreach(cpp_file ${EXEC_VCD_SPLIT_SOURCES})
    #set_source_files_properties(${cpp_file} PROPERTIES COMPILE_FLAGS -g)
    #set_source_files_properties(${cpp_file} PROPERTIES COMPILE_FLAGS -O0)
#endforeach()

#Define library
add_executable(vcd_split ${EXEC_VCD_SPLIT_SOURCES} ${EXEC_VCD_SPLIT_HEADERS})
target_link_libraries(vcd_split)
