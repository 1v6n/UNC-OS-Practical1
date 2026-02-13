cmake_minimum_required(VERSION 3.28)
project(SystemSentinel C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

# Find the required libraries
find_library(PROM_LIB prom REQUIRED)
find_library(PROMHTTP_LIB promhttp REQUIRED)
find_library(MICROHTTPD_LIB microhttpd REQUIRED)

add_executable(SystemSentinel
    include/expose_metrics.h
    include/metrics.h
    src/expose_metrics.c
    src/main.c
    src/metrics.c)

# Link the libraries
target_link_libraries(SystemSentinel ${PROM_LIB} ${PROMHTTP_LIB} ${MICROHTTPD_LIB} pthread)
