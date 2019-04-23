# set minimum cmake version
CMAKE_MINIMUM_REQUIRED(VERSION 2.6)

# create proj
SET(MY_PROJ_NAME "rdmaft")
PROJECT(${MY_PROJ_NAME})

# set target file
SET(MY_TARGET ${MY_PROJ_NAME})

# set C compiler & CXX compiler
SET(CMAKE_C_COMPILER "gcc")

# set executable file output path & lib output path
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)
#SET(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# set install prefix
SET(CMAKE_INSTALL_PREFIX /usr/lib)

# add sub dir
ADD_SUBDIRECTORY(src)
