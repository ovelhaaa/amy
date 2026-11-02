cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "../shared/smk_protocol" "../shared/smk_uart")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(smk-p4-gate0 VERSION 0.1.0)
