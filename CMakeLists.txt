cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Signed OTA builds set PASSPORT_OS_VERSION so the running image can report the
# same release version in Settings → About. Developer builds keep IDF's default.
if(DEFINED ENV{PASSPORT_OS_VERSION} AND NOT "$ENV{PASSPORT_OS_VERSION}" STREQUAL "")
    set(PROJECT_VER "$ENV{PASSPORT_OS_VERSION}")
endif()

project(FoloToy-AI-Passport)
