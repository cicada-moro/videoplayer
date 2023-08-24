#ifndef FFMPEG_H
#define FFMPEG_H

#include <QObject>
#include <QThread>
#include <QMutex>
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavdevice/avdevice.h>
#include <libavformat/version.h>
#include <libavformat/avio.h>
#include <libavutil/time.h>
#include <libavutil/mathematics.h>
#include <libavutil/imgutils.h>
#include <libavutil/pixelutils.h>

#undef main
}

class FFmpeg
{
public:
    FFmpeg();
    void init();
    double openFileCtx(QString str);
    void closeAndFree();
    AVPacket readAVPacket();
    int decodeAVPacket(const AVPacket *pkt);
    bool ToRGB(char *out, int outwidth, int outheight);
    int ToPCM(char *out);

private:
         //ffmpeg相关变量预先定义与分配
    AVFormatContext *m_pAVFormatCtx;    //解封装上下文
    SwsContext  *m_pSwsCtx;             //视频转码上下文
    SwrContext  *m_pSwrCtx;

    AVCodecContext *avCodecCtx;         //解码上下文
    AVFrame *m_pAVFrame = 0;            // ffmpeg单帧缓存
    AVFrame *m_pYuvFrame;
    AVFrame *m_pPcmFrame;
    AVFrame *m_pAVFrameRGB32 = 0;       // ffmpeg单帧缓存转换颜色空间后的缓存
    AVPacket *m_pAVPacket = 0;          // ffmpag单帧数据包

    int gotPicture = 0;          // 解码时数据是否解码成功
    int numBytes = 0;            // 解码后的数据长度
    uchar *outBuffer = 0;        // 解码后的数据存放缓存区
    char m_errorBuff[1024];      //打开时发生的错误信息

    double m_totalMs;               //总时长
    int m_videoStream;           //视频流
    int m_audioStream;           //音频流
    int m_fps;                   //每秒的视频帧数

    int m_pts;                   //获得当前解码帧的时间

    int m_sampleRate;            //样本率
    int m_sampleSize;            //样本大小
    int m_channel;               //通道数

    QMutex m_mutex;
};

#endif // FFMPEG_H
