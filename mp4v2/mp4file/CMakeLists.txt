project(mp4file)
include_directories(../inc)
set(SRC_LIST
	main.cpp
	MP4Encoder.cpp
)
add_executable(mp4file ${SRC_LIST})
target_link_libraries(mp4file F:/mp4v2/lib/libmp4v2.lib)
