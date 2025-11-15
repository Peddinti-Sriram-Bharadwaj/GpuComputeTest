# This script is called by CMakeLists.txt
# It reads a binary .spv file and appends it as a C++ array
# to a header file.
# ARGV0 = input .spv file
# ARGV1 = output .h file

file(READ ${ARGV0} SPV_FILE_HEXA HEX)
string(REGEX MATCHALL "[0-9a-f][0-9a-f]" SPV_FILE_BYTES ${SPV_FILE_HEXA})

set(BYTES_PER_LINE 16)
set(COUNTER 0)
set(LINE_CONTENT "  ")

foreach(BYTE ${SPV_FILE_BYTES})
    string(APPEND LINE_CONTENT "0x${BYTE}, ")
    math(EXPR COUNTER "${COUNTER} + 1")
    if(COUNTER EQUAL BYTES_PER_LINE)
        file(APPEND ${ARGV1} "${LINE_CONTENT}\n")
        set(COUNTER 0)
        set(LINE_CONTENT "  ")
    endif()
endforeach()

if(NOT COUNTER EQUAL 0)
    file(APPEND ${ARGV1} "${LINE_CONTENT}\n")
endif()