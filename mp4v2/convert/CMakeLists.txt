project(convert)
include_directories(../inc)
set(SRC_LIST
	main.cpp
	MP4Encoder.cpp
)

add_executable(convert ${SRC_LIST})
target_link_libraries(convert F:/mp4v2/lib/libmp4v2.lib)
