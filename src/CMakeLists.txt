# set lib head file dir
SET(SELF_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/include")
INCLUDE_DIRECTORIES(${SELF_INCLUDE_DIRECTORIES})

# get all source file
MACRO(SUBDIRLIST result curdir)
    FILE(GLOB_RECURSE children RELATIVE ${curdir} *.c *.cc *.cpp)
    SET(${result} ${children})
ENDMACRO()

SUBDIRLIST(SRC_LIST ${CMAKE_CURRENT_SOURCE_DIR})

# generate target file
ADD_LIBRARY(rdmaft SHARED ${SRC_LIST})
TARGET_LINK_LIBRARIES(rdmaft ibverbs rdmacm pthread)


#install lib
INSTALL(TARGETS rdmaft
        LIBRARY DESTINATION /usr/lib)

#install headfile
INSTALL(FILES ${CMAKE_SOURCE_DIR}/include/rdmaft_recv.h DESTINATION /usr/include)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/include/rdmaft_send.h DESTINATION /usr/include)
