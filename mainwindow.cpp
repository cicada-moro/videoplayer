#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QScreen>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <ffmpeg.h>

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
    while( true){
        AVPacket pkt=video->readAVPacket();
        if(pkt.data==0){
            break;
        }

        int num=video->decodeAVPacket(&pkt);
        char *out=new char[outwidth*outheight*4];//解码后的RGB视频数据
        video->ToRGB(out,outwidth,outheight);

        QImage image((uchar *)out,outwidth,outheight,QImage::Format_RGB32);
        ui->video->setPixmap(QPixmap::fromImage(image));
        ui->video_slider->setValue(num);
        delay(400);
//        image.save(QString("%1.png").arg(outwidth));
    }

}

