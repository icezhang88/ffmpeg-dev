cmake_minimum_required(VERSION 3.16)
project(ffmpegdev)

set(CMAKE_CXX_STANDARD 14)

include_directories(/usr/local/include)
link_directories(/usr/local/lib)

find_package(PkgConfig REQUIRED)
add_executable(ffmpegdev main.cpp)



pkg_check_modules(ffmpeg REQUIRED IMPORTED_TARGET libavcodec libavformat libavutil)

target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::ffmpeg)


#target_link_libraries(${PROJECT_NAME} libavcodec.a )
#
#target_link_libraries(${PROJECT_NAME} libavdevice.a)
#target_link_libraries(${PROJECT_NAME} libavfilter.a)
#target_link_libraries(${PROJECT_NAME} libavformat.a)
#target_link_libraries(${PROJECT_NAME} libavutil.a)
#target_link_libraries(${PROJECT_NAME} libswresample.a)
#target_link_libraries(${PROJECT_NAME} libswscale.a)