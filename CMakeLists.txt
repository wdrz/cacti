cmake_minimum_required (VERSION 3.1)
project (CACTI)

enable_testing()

#set(CMAKE_C_STANDARD ...)
set(CMAKE_C_FLAGS "-g -Wall -Wextra -pthread")

# http://stackoverflow.com/questions/10555706/
macro (add_executable _name)
  # invoke built-in add_executable
  _add_executable(${ARGV})
  if (TARGET ${_name})
    target_link_libraries(${_name} cacti)
  endif()
endmacro()

add_library(cacti STATIC wd417920/cacti.c)
add_library(messages STATIC wd417920/messages.c)
add_library(pool STATIC wd417920/pool.c)
add_executable(macierz wd417920/macierz.c)
add_executable(silnia wd417920/silnia.c)
add_subdirectory(wd417920/test)

install(TARGETS cacti DESTINATION wd417920)
