#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScreen>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <ffmpeg.h>
#include <QAudioSink>
#include <QMediaDevices>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QMediaPlayer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowFlags(Qt::FramelessWindowHint|Qt::WindowSystemMenuHint|Qt::WindowMinimizeButtonHint|Qt::WindowMaximizeButtonHint);
    video_list=new QStringListModel(list);
    ui->listView->setModel(video_list);
    ui->listView->setWordWrap(true);
    ui->listView->setMovement(QListView::Free);

    QPixmap volume_pix(":/image/image/volume.png");
    volume_pix.scaled(ui->volume_icon->size(),Qt::KeepAspectRatio);
    ui->volume_icon->setFixedSize(36,34);
    ui->volume_icon->setPixmap(volume_pix);
    ui->volume_icon->setScaledContents(true);


}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::delay(int msec)//延时函数
{
    QTime dieTime = QTime::currentTime().addMSecs(msec);
    while( QTime::currentTime() < dieTime )
        QCoreApplication::processEvents(QEventLoop::AllEvents, 100);
}


void MainWindow::on_window_closed_clicked()
{
    this->close();
}


void MainWindow::on_window_expanded_clicked()
{
    QScreen *screen=QGuiApplication::primaryScreen();
    QRect deskrect=screen->availableGeometry();
    QIcon icon=ui->window_expanded->icon();
    if(this->width()<deskrect.width()){
        QIcon icon1(":/image/image/small_size.png");
        this->setGeometry(deskrect);
        ui->window_expanded->setIcon(icon1);
    }
    else if(this->width()>=deskrect.width()){
        this->setGeometry(deskrect.width()/2-958/2,deskrect.height()/2-625/2,958,625);
        QIcon icon1(":/image/image/window_enlarged.png");
        ui->window_expanded->setIcon(icon1);
    }
}


void MainWindow::on_window_smalled_clicked()
{
    this->showMinimized();
}

void MainWindow::mousePressEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        //获得鼠标的初始位置
        mouseStartPoint = event->globalPos();
//        mouseStartPoint = event->pos();//会抖动
        //获得窗口的初始位置
        windowTopLeftPoint = this->frameGeometry().topLeft();
        QPoint begin(ui->titlebar->x()+this->x(),ui->titlebar->y()+this->y());
        for(int i=0;i<ui->titlebar->width();i++){
            for(int j=0;j<ui->titlebar->height();j++){
                QPoint p=begin+QPoint(i,j);
                if(mouseStartPoint==p){
                    m_bDrag=true;
                    return;
                }
            }
        }
    }

}

void MainWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if(event->button() == Qt::LeftButton)
    {
        m_bDrag = false;
    }

}

void MainWindow::mouseMoveEvent(QMouseEvent *event)
{
    if(m_bDrag)
    {
        //获得鼠标移动的距离
        QPoint distance = event->globalPos() - mouseStartPoint;
//        QPoint distance = event->pos() - mouseStartPoint;
        //改变窗口的位置
        this->move(windowTopLeftPoint + distance);
    }

}



void MainWindow::on_open_triggered()
{
    QString dir=QCoreApplication::applicationDirPath();
    QString str=QFileDialog::getOpenFileName(this,"打开一个文件",dir,"视频文件(*.mp4);;");
    QFile file(str);
    QFileInfo fileinfo(str);
    ui->title->setText(fileinfo.fileName());
//    qDebug()<<fileinfo.filePath();

    //打开ffmpeg解码
    FFmpeg *video=new FFmpeg();
    double m_totalMs=video->openFileCtx(str);

    ui->video_slider->setRange(0,m_totalMs);

    int outwidth=ui->video->width();
    int outheight=ui->video->height();
    //初始化音频输出设备
    QMediaDevices *m_devices=new QMediaDevices(this);
    QAudioDevice m_device=m_devices->defaultAudioOutput();
    QAudioFormat fmt=m_device.preferredFormat();
    QAudioSink *m_sink=new QAudioSink(m_device,fmt);
    QIODevice *io;
    //IODevice获取本机扬声器
    io=m_sink->start();
    //        qDebug()<<m_sink->state()<<m_sink->error();
    io->open(QIODevice::ReadWrite);

    while( true){
        AVPacket *pkt=video->readAVPacket();
        if(pkt->data==0){
            break;
        }

        int num=video->decodeAVPacket(pkt);
        char *out_video=new char[outwidth*outheight*4];//解码后的RGB视频数据

        char *out_audio=new char[1024*1024];
//        qDebug()<<pkt->stream_index<<video->videoStream();
        if(pkt->stream_index==video->videoStream()){
            video->ToRGB(out_video,outwidth,outheight);

            QImage image((uchar *)out_video,outwidth,outheight,QImage::Format_RGB32);
            ui->video->setPixmap(QPixmap::fromImage(image));
            ui->video_slider->setValue(num);
        }
        else if(pkt->stream_index==video->audioStream()){
            int outsize=video->ToPCM(out_audio);
            //废物字节流写错数据（生气）
//            QByteArray audio_data=QByteArray(out_audio,outsize);

//        qDebug()<<audio_data;

//        旧版本的QAudioFormat
//        qDebug()<<outsize<<audio_data;
//        QAudioFormat fmt;
//        fmt.setSampleRate(44100);//设置采样率
//        fmt.setChannelCount(2);//设置通道数
//        fmt.setSampleFormat(QAudioFormat::Int16);


            io->write(out_audio,outsize);
        }
        delay(25);
//        image.save(QString("%1.png").arg(outwidth));
    }

}

