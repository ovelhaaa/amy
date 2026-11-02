cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "components" "../shared/smk_protocol" "../shared/smk_uart")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(smk-s3-synth)
