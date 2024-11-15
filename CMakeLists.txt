#+++
#
#	DESCRIPTION: Make file for Ethernet-To-Ethernet Forwared
#
#	AUTHOR: Ruslan R. Laishev
#
#	CREATION DATE: 12-NOV-2024
#
#	MODIFICATION HISTORY:
#
#
#---
cmake_minimum_required(VERSION 3.10)
set(CMAKE_VERBOSE_MAKEFILE OFF)
set(CMAKE_COLOR_MAKEFILE   ON)

project(e2e-fwd LANGUAGES C)


set (3RDPARTY_ROOT "${CMAKE_SOURCE_DIR}/3rdparty/")
set (STARLET_ROOT "${3RDPARTY_ROOT}/utility_routines/")
add_subdirectory(${STARLET_ROOT})

set ( INCS
	${STARLET_ROOT}/
)


add_definitions( -D__ARCH__NAME__="${CMAKE_HOST_SYSTEM_PROCESSOR}" )



add_executable(e2e-fwd e2e-fwd.c
)

target_link_libraries(e2e-fwd starlet pthread)
target_include_directories ( e2e-fwd PRIVATE ${INCS} )
add_dependencies(e2e-fwd starlet)


add_custom_target(files_config SOURCES
	e2e-fwd_conf.conf
	README.md
)
