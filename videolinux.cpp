#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#include <libavdevice/avdevice.h>
#include <libavutil/mem.h>
#include "libavutil/opt.h"
#include "libavutil/time.h"

#ifdef __cplusplus
};
#endif




using namespace std;

int main() {
    av_register_all();
    // 获取采集格式
    AVInputFormat *inputFmt = av_find_input_format("video4linux2"); // 视频

    int ret = 0;
    AVFormatContext *fmt_ctx = NULL;
    char *deviceName = "/dev/video0"; // 视频设备
    AVDictionary *options = NULL;

    av_dict_set(&options, "video_size", "640x480", 0);  // 为打开视频设备设置参数
//    av_dict_set(&options, "pixel_format", "yuyv422",0);
    // 打开设备

    ret = avformat_open_input(&fmt_ctx, deviceName, inputFmt, &options);

    ret = avformat_find_stream_info(fmt_ctx, 0);
    if (ret != 0) {
        cout<<"cannot open device"<<endl;
    }

    av_dump_format(fmt_ctx, 0, deviceName, 0);


    return 0;
}




