cmake_minimum_required(VERSION 3.5)

project (vivante-yolov3-pipeline)

set(SOURCES
    src/main.c
)

add_executable(vivante-yolov3-pipeline ${SOURCES})

target_include_directories(vivante-yolov3-pipeline
    PUBLIC 
        /usr/include/glib-2.0/
        /usr/lib/glib-2.0/include/
        /usr/include/nnstreamer/
)

target_link_libraries(vivante-yolov3-pipeline
    -lm
    -lglib-2.0
    -lcapi-nnstreamer)
