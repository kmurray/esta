#
# Executable Source files
#
file(GLOB_RECURSE EXEC_VCD_EXTRACT_SOURCES *.cpp)
file(GLOB_RECURSE EXEC_VCD_EXTRACT_HEADERS *.hpp)

add_executable(vcd_extract ${EXEC_VCD_EXTRACT_SOURCES} ${EXEC_VCD_EXTRACT_HEADERS})

target_link_libraries(vcd_extract gzstream)
