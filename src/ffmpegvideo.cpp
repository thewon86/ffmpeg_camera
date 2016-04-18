#include "ffmpegvideo.h"
#include <QByteArray>
#include <QTimer>

int linesize[4]  = {3 * 320, 0, 0, 0};
FFmpegVideo::FFmpegVideo(QObject *parent) :
    QObject(parent)
{
    av_register_all();
    avdevice_register_all();
    avformat_network_init();
    m_videodata = NULL;
    m_timer = new QTimer;

    rgb = new unsigned char[320 * 240 * 3];
    rgb_sws_ctx = 0;
}

void FFmpegVideo::init(const QString &dev)
{
    openCamera(dev);

    connect(m_timer, SIGNAL(timeout()), this, SLOT(readFrame()));
    m_timer->start(100);

}

bool FFmpegVideo::openCamera(const QString &dev)
{
    AVCodec *pCodec;
    int	i;

    pFrame = av_frame_alloc();
    pFrameYUV=av_frame_alloc();
    pFormatCtx = avformat_alloc_context();

    AVDictionary* options = NULL;
//	Set some options
    av_dict_set(&options,"framerate","15",0);
//	Video frame size. The default is to capture the full screen
    av_dict_set(&options,"video_size","320x240",0);
    AVInputFormat *ifmt=av_find_input_format("video4linux2");
    if(avformat_open_input(&pFormatCtx,dev.toAscii().data()/*"/dev/video0"*/,ifmt,&options)!=0){
        printf("Couldn't open input stream.\n");
        return false;
    }
//获取视频流信息
    if(avformat_find_stream_info(pFormatCtx,NULL)<0)
    {
        printf("Couldn't find stream information.\n");
        return false;
    }
//获取视频流索引
    videoindex=-1;
    for(i=0; i<pFormatCtx->nb_streams; i++){
        if(pFormatCtx->streams[i]->codec->codec_type==AVMEDIA_TYPE_VIDEO)
        {
            videoindex=i;
            break;
        }
    }
    if(videoindex==-1)
    {
        printf("Couldn't find a video stream.\n");
        return false;
    }
//获取视频流的分辨率大小
    pCodecCtx=pFormatCtx->streams[videoindex]->codec;
    printf("pCodecCtx->pix_fmt:%d, pCodecCtx->codec_id:%d, pCodecCtx->width:%d, pCodecCtx->height:%d\n", pCodecCtx->pix_fmt, pCodecCtx->codec_id, pCodecCtx->width, pCodecCtx->height);
    pCodec=avcodec_find_decoder(pCodecCtx->codec_id);
    if(pCodec==NULL)
    {
        printf("Codec not found.\n");
        return false;
    }
    if(avcodec_open2(pCodecCtx, pCodec,NULL)<0)
    {
        printf("Could not open codec.\n");
        return false;
    }

    rgb_sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
                                 pCodecCtx->pix_fmt,
                                 320,
                                 240,
                                 AV_PIX_FMT_RGB24,
                                 SWS_BICUBIC, 0, 0, 0);
    return true;
}

void FFmpegVideo::closeCamera()
{
    sws_freeContext(rgb_sws_ctx);
    delete rgb;
    avcodec_close(pCodecCtx);
    avformat_close_input(&pFormatCtx);
    av_free(pFrame);

}

void FFmpegVideo::readFrame()
{
    AVPacket *packet=(AVPacket *)av_malloc(sizeof(AVPacket));
    int ret, got_picture;
    static int framecnt = 0;

    if(av_read_frame(pFormatCtx, packet)>=0){
        if(packet->stream_index==videoindex){
            ret = avcodec_decode_video2(pCodecCtx, pFrame, &got_picture, packet);
            if(ret < 0){
                printf("Decode Error.\n");
                return;
            }

            if(got_picture){
                framecnt++;
                printf("frame:%d.\n", framecnt);
                if(rgb_sws_ctx != NULL){
                    sws_scale(rgb_sws_ctx, (const uint8_t* const*)pFrame->data, pFrame->linesize, 0, 240,
                              (uint8_t**)&rgb, linesize);
                    m_videodata = new QByteArray;
                    *m_videodata = QByteArray::fromRawData((const char*)rgb, 320 * 240 * 3);

                    emit newFrame(m_videodata);
                }
            }
        }
        av_packet_unref(packet);
    }
}
