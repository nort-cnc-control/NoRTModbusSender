cmake_minimum_required(VERSION 3.13)
project(NoRTModbusProxy)

include(FindPkgConfig)
pkg_search_module(MODBUS REQUIRED libmodbus)

add_executable(nort_mb_proxy main.c)
target_include_directories(nort_mb_proxy PUBLIC ${MODBUS_INCLUDE_DIRS})
target_link_libraries(nort_mb_proxy ${MODBUS_LIBRARIES})

install(TARGETS nort_mb_proxy DESTINATION bin)

