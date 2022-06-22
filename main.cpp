#include <stdio.h>
#include "iostream"

#define __STDC_CONSTANT_MACROS

extern "C"
{
#include "libavutil/opt.h"
#include "libavutil/channel_layout.h"
#include "libavutil/common.h"
#include "libavutil/imgutils.h"
#include "libavutil/mathematics.h"
#include "libavutil/samplefmt.h"
#include "libavutil/time.h"
#include "libavutil/fifo.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/avio.h"
//#include "libavfilter/avfiltergraph.h"
#include "libavfilter/avfilter.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include "libswscale/swscale.h"
#include "libswresample/swresample.h"
#include "libavdevice/avdevice.h"
};

#include <string>
#include <memory>
#include <thread>
#include <iostream>

using namespace std;


AVFormatContext *inputContext = nullptr;
AVFormatContext *outputContext = nullptr;
int64_t lastReadPacketTime = 0;
AVCodec *pdec = new AVCodec;
AVCodecContext *decodeContext;
AVCodecContext *encodeContext;
int audioIndex = -1;
int videoIndex = -1;
uint8_t *pSwpBuffer = nullptr;
struct SwsContext *pSwsContext = nullptr;

class SwsScaleContext {
public:
    SwsScaleContext() {

    }

    void SetSrcResolution(int width, int height) {
        srcWidth = width;
        srcHeight = height;
    }

    void SetDstResolution(int width, int height) {
        dstWidth = width;
        dstHeight = height;
    }

    void SetFormat(AVPixelFormat iformat, AVPixelFormat oformat) {
        this->iformat = iformat;
        this->oformat = oformat;
    }

public:
    int srcWidth;
    int srcHeight;
    int dstWidth;
    int dstHeight;
    AVPixelFormat iformat;
    AVPixelFormat oformat;
};

static int interrupt_cb(void *ctx) {
    int timeout = 10;
    if (av_gettime() - lastReadPacketTime > timeout * 1000 * 1000) {
        return -1;
    }
    return 0;
}

int initSwsContext(struct SwsContext **pSwsContext, SwsScaleContext *swsScaleContext) {
    *pSwsContext = sws_getContext(swsScaleContext->srcWidth, swsScaleContext->srcHeight, swsScaleContext->iformat,
                                  swsScaleContext->dstWidth, swsScaleContext->dstHeight, swsScaleContext->oformat,
                                  SWS_BICUBIC,
                                  NULL, NULL, NULL);
    if (*pSwsContext == NULL) {
        return -1;
    }
    return 0;
}

int initSwsFrame(AVFrame *pSwsFrame, int iWidth, int iHeight) {
    int numBytes = av_image_get_buffer_size(encodeContext->pix_fmt, iWidth, iHeight, 1);
    /*if(pSwpBuffer)
    {
        av_free(pSwpBuffer);
    }*/
    pSwpBuffer = (uint8_t *) av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(pSwsFrame->data, pSwsFrame->linesize, pSwpBuffer, encodeContext->pix_fmt, iWidth, iHeight, 1);
    pSwsFrame->width = iWidth;
    pSwsFrame->height = iHeight;
    pSwsFrame->format = encodeContext->pix_fmt;
    return 1;
}

int OpenInput(string inputUrl) {

    inputContext = avformat_alloc_context();

    AVDictionary *options = nullptr;
    inputContext->interrupt_callback.callback = interrupt_cb;
 
    AVInputFormat *ifmt = av_find_input_format("video4linux2"); //video4linux2
    av_dict_set_int(&options, "rtbufsize", 100, 0);
    av_dict_set(&options, "rtsp_transport", "udp", 0);
    av_dict_set(&options, "stimeout", "200000", 0);
    av_dict_set(&options, "fflags", "nobuffer", 0);

    //apple
    // AVInputFormat *ifmt = av_find_input_format("avfoundation"); //video4linux2
    // av_dict_set_int(&options, "rtbufsize", 18432000, 0);
    // av_dict_set_int(&options, "framerate", 30, 0);
    //av_dict_set_int(&options, "video_size", "640x480", 0);
 
    lastReadPacketTime = av_gettime();
    int ret = avformat_open_input(&inputContext, inputUrl.c_str(), ifmt, &options);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Input file open input failed\n");
        return ret;
    }
    //增加延迟
//    ret = avformat_find_stream_info(inputContext, nullptr);
//    if (ret < 0) {
//        av_log(NULL, AV_LOG_ERROR, "Find input file stream inform failed\n");
//    } else {
//        av_log(NULL, AV_LOG_ERROR, "Open input file  %s success\n", inputUrl.c_str());
//    }

    for (int i = 0; i < inputContext->nb_streams; i++) {
        if (inputContext->streams[i]->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO) {
            videoIndex = i;
        } else if (inputContext->streams[i]->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO) {
            audioIndex = i;
        }
    }

    return ret;
}


AVPacket *ReadPacketFromSource() {
    AVPacket *packet = new AVPacket;
    av_init_packet(packet);
    lastReadPacketTime = av_gettime();
    int ret = av_read_frame(inputContext, packet);
    if (ret >= 0) {
        return packet;
    } else {
        av_packet_unref(packet);
        delete packet;
        packet = nullptr;
        return nullptr;
    }
}

int initVideoDecodeCodec() {
    int ret = av_find_best_stream(inputContext, AVMEDIA_TYPE_VIDEO, -1, -1, &pdec, 0);

    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Find inform failed\n");
        return ret;
    }
    decodeContext = avcodec_alloc_context3(pdec);
    avcodec_parameters_to_context(decodeContext, inputContext->streams[videoIndex]->codecpar);
    ret = avcodec_open2(decodeContext, pdec, NULL);
    return ret;
}

int initVideoEncodeCodec() {

    AVDictionary *params = NULL;
    av_dict_set(&params, "preset", "superfast", 0);
    av_dict_set(&params, "tune", "zerolatency", 0);
    av_dict_set(&params, "profile", "baseline", 0);
    av_dict_set(&params, "cbr", "2000", 0);
//    av_dict_set(&params, "threads", "auto", 0);

    AVCodec *picCodec;
    auto inputStream = inputContext->streams[videoIndex];

    picCodec = avcodec_find_encoder(AV_CODEC_ID_H264);
    encodeContext = avcodec_alloc_context3(picCodec);

    encodeContext->codec_id = picCodec->id;
    encodeContext->time_base.num = inputStream->time_base.num;
    encodeContext->time_base.den = inputStream->time_base.den;
    //encodeContext->pix_fmt=decodeContext->pix_fmt;
    encodeContext->pix_fmt = *picCodec->pix_fmts;

    encodeContext->width = inputStream->codecpar->width;
    encodeContext->height = inputStream->codecpar->height;
    encodeContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

    int ret = avcodec_open2(encodeContext, picCodec, &params);
    if (ret < 0) {
        std::cout << "open video codec failed" << endl;
        return ret;
    }
    return 1;
}

int WritePacket(AVPacket *packet) {
    auto inputStream = inputContext->streams[packet->stream_index];
    auto outputStream = outputContext->streams[packet->stream_index];
    av_packet_rescale_ts(packet, inputStream->time_base, outputStream->time_base);
    return av_write_frame(outputContext, packet);
}


int OpenOutput(string outUrl) {
    int ret = avformat_alloc_output_context2(&outputContext, nullptr, "flv", outUrl.c_str());
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "open output context failed\n");
        goto Error;
    }

    ret = avio_open2(&outputContext->pb, outUrl.c_str(), AVIO_FLAG_WRITE, nullptr, nullptr);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "open avio failed");
        goto Error;
    }

    for (int i = 0; i < inputContext->nb_streams; i++) {

        if (inputContext->streams[i]->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO) {
            AVStream *stream = avformat_new_stream(outputContext, nullptr);
            stream->codecpar->codec_tag = 0;
            avcodec_parameters_from_context(stream->codecpar, encodeContext);
        } else continue;

        if (ret < 0) {
            av_log(NULL, AV_LOG_FATAL, "copy codecpar context failed");
            goto Error;
        }
    }

    ret = avformat_write_header(outputContext, nullptr);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "format write header failed");
        goto Error;
    }

    av_log(NULL, AV_LOG_FATAL, " Open output file success %s\n", outUrl.c_str());
    return ret;
    Error:
    if (outputContext != nullptr) {
        if (!(outputContext->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&outputContext->pb);
        }
        avformat_free_context(outputContext);
        outputContext = nullptr;
    }
    return ret;
}

bool DecodeVideo(AVPacket *packet, AVFrame *frame) {
    int ret = avcodec_send_packet(decodeContext, packet);
    if (ret < 0) return false;

    av_packet_unref(packet);
    delete packet;
    packet = nullptr;
    ret = avcodec_receive_frame(decodeContext, frame);
    return ret >= 0 ? true : false;
}

AVPacket *EncodeVideo(AVFrame *frame) {
    int ret = 0;


    ret = avcodec_send_frame(encodeContext, frame);
    if (ret < 0) nullptr;

    AVPacket *packet = new AVPacket;
    av_init_packet(packet);
    ret = avcodec_receive_packet(encodeContext, packet);
    if (ret >= 0) return packet;
    av_packet_unref(packet);
    delete packet;
    packet = nullptr;
    return nullptr;
}

void CloseInput() {
    if (inputContext != nullptr) {
        avformat_close_input(&inputContext);
    }
    if (pSwsContext) {
        sws_freeContext(pSwsContext);
    }
}

void CloseOutput() {
    if (outputContext != nullptr) {
        av_write_trailer(outputContext);
        avformat_free_context(outputContext);
    }
}

void Init() {
    //av_register_all();
    //avfilter_register_all();
    avdevice_register_all();
    avformat_network_init();
    av_log_set_level(AV_LOG_WARNING);
}

int main() {
    Init();
    int64_t startTime = 0;
    int printCount = 0;
    SwsScaleContext swsScaleContext;
    AVFrame *pSwsVideoFrame = av_frame_alloc();
    AVFrame *frame = av_frame_alloc();
    int ret = OpenInput(
            "/dev/video0"); ///dev/video0 rtsp://admin:CGKJ12345@192.168.1.108:554/cam/realmonitor?channel=1&subtype=0
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Open input file failed");
        goto End;
    }
    ret = initVideoDecodeCodec();//
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Init decode codec failed");
        goto End;
    }
    ret = initVideoEncodeCodec();
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Init encode codec failed");
        goto End;
    }

    if (ret >= 0) {
        ret = OpenOutput("rtmp://k2.bodyta.com:1935/live/livestream");///home/wgg_126/wgg/test3.flv
    }
    if (ret < 0) goto End;

    startTime = av_gettime();

    swsScaleContext.SetSrcResolution(inputContext->streams[0]->codecpar->width,
                                     inputContext->streams[0]->codecpar->height);
    swsScaleContext.SetDstResolution(encodeContext->width, encodeContext->height);

    swsScaleContext.SetFormat(inputContext->streams[0]->codec->pix_fmt, encodeContext->pix_fmt);
    initSwsContext(&pSwsContext, &swsScaleContext);
    initSwsFrame(pSwsVideoFrame, encodeContext->width, encodeContext->height);

    while (true) {
        auto packet = ReadPacketFromSource();
        if (packet) {
            if (packet->stream_index == videoIndex) {
                bool ret = DecodeVideo(packet, frame);
                if (ret) {
                    sws_scale(pSwsContext, (const uint8_t *const *) frame->data,
                              frame->linesize, 0, inputContext->streams[0]->codecpar->height,
                              (uint8_t *const *) pSwsVideoFrame->data, pSwsVideoFrame->linesize);

                    pSwsVideoFrame->pts = frame->pts;
                    AVPacket *packetEncode = EncodeVideo(pSwsVideoFrame);
                    if (packetEncode) {
                        ret = WritePacket(packetEncode);
                        if (printCount++ == 0) {
                            cout << "write packet:" << ret << endl;
                        }

                    }
                }
            }
        }
    }
    av_frame_unref(pSwsVideoFrame);
    delete pSwsVideoFrame;
    cout << "write packet end" << ret << endl;
    End:
    if (outputContext) {
        av_write_trailer(outputContext);
        avformat_free_context(outputContext);
    }
    while (true) {
        this_thread::sleep_for(chrono::seconds(100));
    }
    return 0;
}
