set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components/elf_loader" "${CMAKE_CURRENT_LIST_DIR}/../../components/native_exec_pool")
cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(native_elf_c3_poc)
