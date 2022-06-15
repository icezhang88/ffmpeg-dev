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
using namespace std;
class SwsScaleContext
{
public:
    SwsScaleContext()
    {

    }
    void SetSrcResolution(int width, int height)
    {
        srcWidth = width;
        srcHeight = height;
    }

    void SetDstResolution(int width, int height)
    {
        dstWidth = width;
        dstHeight = height;
    }
    void SetFormat(AVPixelFormat iformat, AVPixelFormat oformat)
    {
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

int main()
{
    int64_t lastReadPacketTime = 0;
    AVFrame *pSwsVideoFrame = av_frame_alloc();
    AVFormatContext *inputContext = NULL;
    AVFormatContext *outputContext = NULL;
    AVPacket pkt;
    AVFrame *pFrame, *pFrameYUV;
    SwsContext *pImgConvertCtx;
    AVDictionary *params = NULL;
    AVCodec *pCodec;
    AVCodecContext *pCodecCtx;
    unsigned char *outBuffer;
    AVCodecContext *encodeContext;
    AVCodec *pH264Codec;
    AVDictionary *options = NULL;
    struct SwsContext* pSwsContext = nullptr;
    SwsScaleContext swsScaleContext;
    uint8_t * pSwpBuffer = nullptr;
    int ret = 0;
    unsigned int i = 0;
    int videoIndex = -1;
    int audioIndex=-1;
    int frameIndex = 0;

    const char *inFilename = "/dev/video0";//输入URL
    const char *outFilename = "rtmp://test.bodyta.com:1935/live/pi"; //输出URL
    const char *ofmtName = NULL;

    avdevice_register_all();
    avformat_network_init();
    lastReadPacketTime = av_gettime();
    AVInputFormat *ifmt = av_find_input_format("video4linux2");
    inputContext=avformat_alloc_context();

    // 1. 打开输入
    // 1.1 打开输入文件，获取封装格式相关信息
    av_dict_set_int(&options, "rtbufsize", 18432000  , 0);
    ret = avformat_open_input(&inputContext, inFilename, ifmt, &options);

    if(ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Input file open input failed\n");
        return  ret;
    }
    ret=avformat_find_stream_info(inputContext, nullptr);
    if(ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Find input file stream inform failed\n");
    }



    for (int i = 0; i < inputContext->nb_streams; i++)
    {
        if (inputContext->streams[i]->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO)
        {
            videoIndex = i;
        }
        else if (inputContext->streams[i]->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_AUDIO)
        {
            audioIndex = i;
        }
    }


    printf("%s:%d, videoIndex = %d\n", __FUNCTION__, __LINE__, videoIndex);

    av_dump_format(inputContext, 0, inFilename, 0);


    av_find_best_stream(inputContext,AVMEDIA_TYPE_VIDEO,-1,-1,&pCodec,0);
    if (ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "Find inform failed\n");
        return ret;
    }

    pCodecCtx=avcodec_alloc_context3(pCodec);
    avcodec_parameters_to_context(pCodecCtx, inputContext->streams[videoIndex]->codecpar);
    //  1.5 打开输入解码器
    if (avcodec_open2(pCodecCtx, pCodec, NULL) < 0)
    {
        printf("can't open codec\n");
    }

    AVCodec* picCodec;
    auto inputstream=inputContext->streams[videoIndex];
    picCodec=avcodec_find_encoder(AV_CODEC_ID_H264);
    encodeContext=avcodec_alloc_context3(picCodec);
    encodeContext->codec_id=picCodec->id;
    encodeContext->time_base.num=inputstream->time_base.num;
    encodeContext->time_base.den=inputstream->time_base.den;
    encodeContext->pix_fmt=*picCodec->pix_fmts;

    encodeContext->width=inputstream->codecpar->width;
    encodeContext->height=inputstream->codecpar->height;
    encodeContext->flags|=AV_CODEC_FLAG_GLOBAL_HEADER;
    ret=avcodec_open2(encodeContext,picCodec, nullptr);
    if (ret < 0)
    {
        cout<<"open video codec failed"<<endl;
        return  ret;
    }

    avformat_alloc_output_context2(&outputContext, nullptr,"flv",outFilename);
    if(ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "open output context failed\n");

    }

    ret = avio_open2(&outputContext->pb, outFilename, AVIO_FLAG_WRITE,nullptr, nullptr);
    if(ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "open avio failed");

    }

    for (int i = 0; i < inputContext->nb_streams; i++)
    {

        if (inputContext->streams[i]->codecpar->codec_type == AVMediaType::AVMEDIA_TYPE_VIDEO)
        {
            AVStream * stream = avformat_new_stream(outputContext, nullptr);
            stream->codecpar->codec_tag = 0;
            avcodec_parameters_from_context(stream->codecpar, encodeContext);
        }
        else continue;

        if (ret < 0)
        {
            av_log(NULL, AV_LOG_FATAL, "copy codecpar context failed");

        }
    }
    ret = avformat_write_header(outputContext, nullptr);
    if(ret < 0)
    {
        av_log(NULL, AV_LOG_ERROR, "format write header failed");

    }

    av_log(NULL, AV_LOG_FATAL, " Open output file success %s\n",outFilename);
    //-------------------
    swsScaleContext.SetSrcResolution(inputContext->streams[0]->codecpar->width, inputContext->streams[0]->codecpar->height);
    swsScaleContext.SetDstResolution(encodeContext->width,encodeContext->height);
    swsScaleContext.SetFormat(inputContext->streams[0]->codec->pix_fmt, encodeContext->pix_fmt);


    pSwsContext = sws_getContext(swsScaleContext.srcWidth, swsScaleContext.srcHeight, swsScaleContext.iformat,
                                  swsScaleContext.dstWidth, swsScaleContext.dstHeight, swsScaleContext.oformat,
                                  SWS_BICUBIC,
                                  NULL, NULL, NULL);
    if (pSwsContext == NULL)
    {
        return -1;
    }


    int numBytes = av_image_get_buffer_size(encodeContext->pix_fmt, encodeContext->width, encodeContext->width, 1);
    /*if(pSwpBuffer)
    {
        av_free(pSwpBuffer);
    }*/
    pSwpBuffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(pSwsVideoFrame->data, pSwsVideoFrame->linesize, pSwpBuffer, encodeContext->pix_fmt, encodeContext->width, encodeContext->height, 1);
    pSwsVideoFrame->width =  encodeContext->width;
    pSwsVideoFrame->height = encodeContext->height;
    pSwsVideoFrame->format = encodeContext->pix_fmt;
    AVFrame* frame = av_frame_alloc();
    while(true)
    {
        AVPacket *packet = new AVPacket;
        av_init_packet(packet);
        lastReadPacketTime = av_gettime();
        int ret = av_read_frame(inputContext, packet);


        if(packet)
        {
            if (packet->stream_index == videoIndex)
            {

                int ret = avcodec_send_packet(pCodecCtx, packet);
                if (ret < 0) return false;

                av_packet_unref(packet);
                delete packet;
                packet = nullptr;
                ret = avcodec_receive_frame(pCodecCtx, frame);
                if (ret)
                {
                    sws_scale(pSwsContext, (const uint8_t *const *)frame->data,
                              frame->linesize, 0, inputContext->streams[0]->codecpar->height, (uint8_t *const *)pSwsVideoFrame->data, pSwsVideoFrame->linesize);

                    pSwsVideoFrame->pts = frame->pts;


                    ret = avcodec_send_frame(encodeContext, frame);
                    if (ret < 0) nullptr;

                    AVPacket * packet = new AVPacket;
                    av_init_packet(packet);
                    ret = avcodec_receive_packet(encodeContext, packet);

                    av_packet_unref(packet);
                    delete packet;
                    packet = nullptr;

                    if (packet)
                    {
                        auto inputStream = inputContext->streams[packet->stream_index];
                        auto outputStream = outputContext->streams[packet->stream_index];
                        av_packet_rescale_ts(packet,inputStream->time_base,outputStream->time_base);
                       int printCount=av_write_frame(outputContext, packet);
                        if(printCount++ == 0)
                        {
                            cout <<"write packet:"<< ret <<endl;
                        }

                    }
                }
            }
        }
    }
    av_frame_unref(pSwsVideoFrame);
    delete pSwsVideoFrame;



    return 0;
}