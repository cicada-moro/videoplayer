#include "ffmpeg.h"
#include <QDebug>
#include <QImage>

FFmpeg::FFmpeg()
{
    init();
}

void FFmpeg::init()
{
    m_errorBuff[0] = '\0';
//    av_register_all(); //注册FFMpeg的库
    m_pAVFormatCtx = NULL;//解封装上下文
    m_pYuvFrame = NULL;//解码后的视频帧数据
    m_pPcmFrame = NULL;//解码后的音频数据
    m_pSwsCtx = NULL;//视频转码上下文
    m_pSwrCtx = NULL;//音频重采样上下文

    m_totalMs = 0;//总时长
    m_videoStream = 0;//视频流
    m_audioStream = 1;//音频流
    m_fps = 0;//每秒的视频帧数

    m_pts = 0;//获得当前解码帧的时间

    m_sampleRate = 48000;//样本率
    m_sampleSize = 16;//样本大小
    m_channel = 2;///通道数

//    m_isPlay = false;

}



double FFmpeg::openFileCtx(QString str)
{
    const char *path=str.toStdString().c_str();
    qDebug()<<path;
    closeAndFree(); //先关闭并清理.

//    m_mutex.lock();//锁
    m_pAVFormatCtx=avformat_alloc_context();
    int ret = avformat_open_input(&m_pAVFormatCtx, path, 0, 0);//打开解封装器
    if (ret != 0)//打开错误时
    {
//        m_mutex.unlock();//解锁
        av_strerror(ret, m_errorBuff, sizeof(m_errorBuff));//错误信息
        qDebug()<<"open is false"<<m_errorBuff;
        return 0;
    }
    //获取流信息
    if(avformat_find_stream_info(m_pAVFormatCtx,NULL)<0){
        qDebug()<<"error:nofile";
    }
    m_totalMs = (double)m_pAVFormatCtx->duration / double(AV_TIME_BASE);//获取视频的总时间
//    qDebug()<<m_pAVFormatCtx->duration<<"--------"<<m_totalMs;
    //打印视频详细信息
    av_dump_format(m_pAVFormatCtx, 0, path, 0);
    //解码器
    for (unsigned int i = 0; i < m_pAVFormatCtx->nb_streams; i++)
    {
//        qDebug()<<m_pAVFormatCtx[i].streams;
        AVCodecParameters *avcdecpar = m_pAVFormatCtx->streams[i]->codecpar;//解码上下文
//        AVCodecContext *avCodecCtx=nullptr;
//        qDebug()<<avCodecCtx->codec_type;
        if (avcdecpar->codec_type == AVMEDIA_TYPE_VIDEO)//判断是否为视频
        {
            m_videoStream = i;
            m_fps = av_q2d(m_pAVFormatCtx->streams[i]->avg_frame_rate);//获得每秒的视频帧数
//            qDebug()<<m_fps;
            const AVCodec *avCodec = avcodec_find_decoder(avcdecpar->codec_id);//查找解码器
            if (!avCodec)//未找到解码器
            {
//                m_mutex.unlock();
                qDebug()<<"video code not find\n";
                return 0;
            }
            avCodecCtx=avcodec_alloc_context3(avCodec);//初始化
            avcodec_parameters_to_context(avCodecCtx,avcdecpar);
            int err = avcodec_open2(avCodecCtx, avCodec, NULL);//打开解码器
            if (err != 0)//未打开解码器
            {
//                m_mutex.unlock();
                char buf[1024] = { 0 };
                av_strerror(err, buf, sizeof(buf));
                qDebug()<<"not open"<<buf;
                return 0;
            }
            qDebug()<<"open video codec success!\n";

        }
        else if (avcdecpar->codec_type == AVMEDIA_TYPE_AUDIO)//若为音频流
        {
            m_audioStream = i;//音频流
            const AVCodec *avCodec = avcodec_find_decoder(avcdecpar->codec_id);//查找解码器
            avCodecCtx=avcodec_alloc_context3(avCodec);//初始化
            avcodec_parameters_to_context(avCodecCtx,avcdecpar);
            if (avcodec_open2(avCodecCtx, avCodec, NULL) < 0)
            {
//                m_mutex.unlock();
                return 0;
            }
            m_sampleRate = avCodecCtx->sample_rate;//样本率
            m_channel = avCodecCtx->channels;//通道数
            switch (avCodecCtx->sample_fmt)//样本大小
            {
            case AV_SAMPLE_FMT_S16://signed 16 bits
                m_sampleSize = 16;
                break;
            case  AV_SAMPLE_FMT_S32://signed 32 bits
                m_sampleSize = 32;
            default:
                break;
            }
            //printf("audio sample rate:%d sample size:%d chanle:%d\n",this->sampleRate,this->sampleSize,this->channel);
            qDebug()<<"open audio codec success!\n";

        }

    }//至此为打开解码器过程

    qDebug()<<QString("file totalSec is %1-%2\n").arg( m_totalMs / 60).arg( (int)m_totalMs % 60);//以分秒计时
//    m_mutex.unlock();
    return m_totalMs;
}


void FFmpeg::closeAndFree()
{
//    m_mutex.lock();//需要上锁，以防多线程中你这里在close，另一个线程中在读取，
    if (m_pAVFormatCtx) avformat_free_context(m_pAVFormatCtx);//关闭上下文空间
    if (m_pYuvFrame) av_frame_free(&m_pYuvFrame);//关闭时释放解码后的视频帧空间
    if (m_pPcmFrame) av_frame_free(&m_pPcmFrame);//关闭时释放解码后的音频空间
    if (m_pSwsCtx)
    {
        sws_freeContext(m_pSwsCtx);//释放转码器上下文空间
        m_pSwsCtx = NULL;
    }
    if (m_pSwrCtx)
    {
        swr_free(&m_pSwrCtx);//释放音频上下文空间
    }

//    m_mutex.unlock();
}

//读取视频的每一帧
AVPacket FFmpeg::readAVPacket()
{
    AVPacket pkt;
    memset(&pkt, 0, sizeof(AVPacket));
//    m_mutex.lock();
    if (!m_pAVFormatCtx)
    {
//        m_mutex.unlock();
        return pkt;
    }
    int err = av_read_frame(m_pAVFormatCtx, &pkt);//读取视频帧
    if (err != 0)//读取失败
    {
        av_strerror(err, m_errorBuff, sizeof(m_errorBuff));
    }
//    m_mutex.unlock();
    return pkt;
}

//读取到每帧数据后需要对其进行解码,返回它的pts
int FFmpeg::decodeAVPacket(const AVPacket *pkt)
{
//    m_mutex.lock();
    if (!m_pAVFormatCtx)//若未打开视频
    {
//        m_mutex.unlock();
        return -1;

    }
    if (m_pYuvFrame == NULL)//申请解码的对象空间
    {
        m_pYuvFrame = av_frame_alloc();
    }
    if (m_pPcmFrame == NULL)
    {
        m_pPcmFrame = av_frame_alloc();
    }
//    AVFrame *frame=av_frame_alloc();
    AVFrame *frame = m_pYuvFrame;//此时的frame是解码后的视频流
    if (pkt->stream_index == m_audioStream)//若为音频
    {
        frame = m_pPcmFrame;//此时frame是解码后的音频流
    }
    avcodec_parameters_to_context(avCodecCtx,m_pAVFormatCtx->streams[pkt->stream_index]->codecpar);
    int ret = avcodec_send_packet(avCodecCtx, pkt);//发送之前读取的pkt
    if (ret != 0)
    {
//        m_mutex.unlock();
        return -1;
    }
    ret = avcodec_receive_frame(avCodecCtx, frame);//解码pkt后存入yuv中
    if (ret != 0)
    {
//        m_mutex.unlock();
        return -1;
    }
//    if (pkt->stream_index == m_videoStream){
//        m_pYuvFrame=frame;
//    }
//    else if (pkt->stream_index == m_audioStream)//若为音频
//    {
//        m_pPcmFrame=frame;//此时frame是解码后的音频流
//    }
//    qDebug() << "pts=" << frame->pts<<m_pYuvFrame->pts;

//    m_mutex.unlock();
    int p = frame->pts*av_q2d(m_pAVFormatCtx->streams[pkt->stream_index]->time_base);//当前解码的显示时间
    if (pkt->stream_index == m_audioStream)//为音频流时设置pts
        m_pts = p;
//    qDebug()<<m_pts;
    return p;
}


bool FFmpeg::ToRGB(char *out, int outwidth, int outheight)
{
//    m_mutex.lock();
    if (!m_pAVFormatCtx || !m_pYuvFrame)//未打开视频文件或者未解码
    {
//        m_mutex.unlock();
        return false;
    }
    AVCodecContext *videoCtx=nullptr;
    AVCodecParameters *avcdecpar=m_pAVFormatCtx->streams[m_videoStream]->codecpar;
    const AVCodec *avCodec = avcodec_find_decoder(avcdecpar->codec_id);//查找解码器
    videoCtx=avcodec_alloc_context3(avCodec);//初始化
    avcodec_parameters_to_context(videoCtx,avcdecpar);
    m_pSwsCtx = sws_getContext(videoCtx->width,//初始化一个SwsContext
                                     videoCtx->height,
                                     videoCtx->pix_fmt, //输入像素格式
                                     outwidth, outheight,
                                     AV_PIX_FMT_BGRA,//输出像素格式
                                     SWS_BICUBIC,//转码的算法
                                     NULL, NULL, NULL);

    if (!m_pSwsCtx)
    {
//        m_mutex.unlock();
        qDebug()<<"sws_getCachedContext  failed!\n";
        return false;
    }

    uint8_t *data[AV_NUM_DATA_POINTERS] = { 0 };
    data[0] = (uint8_t *)out;//第一位输出RGB
    int linesize[AV_NUM_DATA_POINTERS] = { 0 };

    linesize[0] = outwidth * 4;//一行的宽度，32位4个字节

//    uint8_t *data=(unsigned char *) av_malloc(
//    av_image_get_buffer_size(AV_PIX_FMT_BGRA,videoCtx->width,videoCtx->height,1));
//    m_pAVFrameRGB32=av_frame_alloc();
//    av_image_fill_arrays(m_pAVFrameRGB32->data,m_pAVFrameRGB32->linesize,data,AV_PIX_FMT_BGRA,outwidth,outheight,1);


    qDebug()<<m_pYuvFrame->data<<m_pYuvFrame->linesize<<m_pYuvFrame->pts;
    int h = sws_scale(m_pSwsCtx, m_pYuvFrame->data, //当前处理区域的每个通道数据指针
                      m_pYuvFrame->linesize,//每个通道行字节数
                      0, videoCtx->height,//原视频帧的高度
                      data,//输出的每个通道数据指针
                      linesize//每个通道行字节数
                      );//开始转码

//    m_pAVFrameRGB32=av_frame_alloc();
//    av_image_fill_arrays(data,linesize,(uint8_t*)m_pAVFrameRGB32,AV_PIX_FMT_BGRA,outwidth,outheight,16);
//    qDebug()<<m_pAVFrameRGB32->data[0]<<m_pAVFrameRGB32->linesize[0];


//    out=(char *)m_pAVFrameRGB32->data[0];
//    av_frame_free(&m_pAVFrameRGB32);

    if (h > 0)
    {
        qDebug()<<"height:"<<h;
    }
//    m_mutex.unlock();
    return true;
}

int FFmpeg::ToPCM(char *out)
{
    m_mutex.lock();
    if (!m_pAVFormatCtx || !m_pPcmFrame || !out)//文件未打开，解码器未打开，无数据
    {
        m_mutex.unlock();
        return 0;
    }

    AVCodecContext *avCodecCtx_Audio =nullptr;
    avcodec_parameters_to_context(avCodecCtx_Audio,m_pAVFormatCtx->streams[m_audioStream]->codecpar);//音频解码器上下文
    if(m_pSwrCtx == NULL)
    {
        m_pSwrCtx = swr_alloc();//初始化
        swr_alloc_set_opts(m_pSwrCtx,avCodecCtx_Audio->channel_layout,AV_SAMPLE_FMT_S16,
                           avCodecCtx_Audio->sample_rate,
                           avCodecCtx_Audio->channels,
                           avCodecCtx_Audio->sample_fmt,
                           avCodecCtx_Audio->sample_rate, 0,0);

        swr_init(m_pSwrCtx);
    }
    uint8_t  *data[1];
    data[0] = (uint8_t *)out;
    int len = swr_convert(m_pSwrCtx, data, 10000, (const uint8_t **)m_pPcmFrame->data, m_pPcmFrame->nb_samples);
    if(len <= 0)
    {
        m_mutex.unlock();
        return 0;
    }

    int outsize = av_samples_get_buffer_size(NULL, avCodecCtx_Audio->channels,
                                             m_pPcmFrame->nb_samples,AV_SAMPLE_FMT_S16,0);

    m_mutex.unlock();

    return outsize;
}
