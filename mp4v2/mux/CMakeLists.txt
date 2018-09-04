project(mux)
include_directories(../inc)
set(SRC_LIST
        main.cpp
)
add_executable(mux ${SRC_LIST})
target_link_libraries(mux F:/mp4v2/lib/libmp4v2.lib)
