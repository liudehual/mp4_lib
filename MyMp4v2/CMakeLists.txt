project(MyMp4v2)


#add_subdirectory(BaseOS)

include_directories(./inc)
include_directories(./BaseOS)
set(SRC_LIST
        main.cpp
        MP4Encoder.cpp
        #MP4Encoder1.cpp
)

add_executable(MyMp4v2 ${SRC_LIST})

target_link_libraries(MyMp4v2
    F:/MP4V2/MyMp4v2/lib/libmp4v2.lib # 必须要用绝对路径?
)
