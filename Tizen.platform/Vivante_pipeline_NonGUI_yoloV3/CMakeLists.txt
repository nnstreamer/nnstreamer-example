cmake_minimum_required(VERSION 3.5)

project (vivante-yolov3-pipeline)

set(SOURCES
    src/main.c
)

include(FindPkgConfig)
pkg_check_modules(VIVANTE_REQUIRE_PKGS REQUIRED glib-2.0 capi-ml-inference)

add_executable(vivante-yolov3-pipeline ${SOURCES})

target_include_directories(vivante-yolov3-pipeline
    PUBLIC 
        ${VIVANTE_REQUIRE_PKGS_INCLUDE_DIRS}
)

target_link_libraries(vivante-yolov3-pipeline
    ${VIVANTE_REQUIRE_PKGS_LDFLAGS}
)
