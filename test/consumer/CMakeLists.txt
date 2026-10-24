# SPDX-License-Identifier: GPL-2.0-or-later OR LGPL-2.1-or-later OR Apache-2.0
# Copyright (C) 2026 Uoc Azizah

# yupiter - a program that consumes the installed package
#
# This project is not part of the yupiter build.  The install test
# builds and installs yupiter into a scratch prefix, configures this
# project against that prefix, builds it and runs it - which is what
# proves the installed package is actually usable: find_package finds
# the configuration file, the imported target carries the include
# directory and the archive with it, and the resulting program links and
# runs without spelling out a single path.

cmake_minimum_required(VERSION 3.16)

project(yupiter-consumer LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

find_package(yupiter REQUIRED)

add_executable(consumer main.c)
target_link_libraries(consumer PRIVATE yupiter::yupiter)
