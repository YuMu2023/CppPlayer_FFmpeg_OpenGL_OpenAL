#include "AVPlayer.h"



/**
* @Author:       Li
* @Version:      1.0
* @Date:         2025-03-26
* @Description:  AVPlayer.h的实现
**/



#include<QPushButton>
#include<QLineEdit>
#include<QVBoxLayout>
#include<QHBoxLayout>
#include<QFileDialog>
#include<QMessageBox>
#include<QCheckBox>
#include<QTimer>
#include<QLabel>

#include"CppPlayer.h"



/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        默认构造函数
* @Param:        @parent (QWidget *) 设置类parent
* @Return:       void
**/
AVPlayer::AVPlayer(QWidget* parent):QWidget(parent){

    lineEdit_path = new QLineEdit;
    pushButton_open = new QPushButton;
    pushButton_browse = new QPushButton;
    pushButton_back = new QPushButton;
    pushButton_advance = new QPushButton;
    pushButton_pause = new QPushButton;
    pushButton_restart = new QPushButton;
    checkBox_loop = new QCheckBox;
    label_av = new QLabel;
    hLayout_path = new QHBoxLayout;
    hLayout_operate = new QHBoxLayout;
    vLayout_main = new QVBoxLayout;
    glWidget = new CppPlayer(this);

    pushButton_open->setText("Open");
    pushButton_browse->setText("Browse");
    pushButton_back->setText("Back");
    pushButton_advance->setText("Advance");
    pushButton_pause->setText("Pause");
    pushButton_restart->setText("Restart");
    checkBox_loop->setText("Loop");
    label_av->setText("A/V:");
    label_av->setMaximumHeight(20);

    hLayout_path->addWidget(lineEdit_path, 4);
    hLayout_path->addWidget(pushButton_open, 2);
    hLayout_path->addWidget(pushButton_browse, 2);
    hLayout_path->addWidget(checkBox_loop, 1);
    hLayout_operate->addWidget(pushButton_back, 2);
    hLayout_operate->addWidget(pushButton_pause, 2);
    hLayout_operate->addWidget(pushButton_advance, 2);
    hLayout_operate->addWidget(pushButton_restart, 2);
    hLayout_operate->addWidget(label_av, 1);
    vLayout_main->addWidget(glWidget, 8);
    vLayout_main->addLayout(hLayout_path, 1);
    vLayout_main->addLayout(hLayout_operate, 1);
    vLayout_main->setMargin(0);

    this->setLayout(vLayout_main);
    this->resize(800, 600);
    this->make_connections();

    QTimer* avT = new QTimer(this);
    connect(avT, &QTimer::timeout, this, &AVPlayer::label_av_update);
    avT->start(200);

}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        稀构函数
* @Param:        void
* @Return:       void
**/
AVPlayer::~AVPlayer(){
    if(this->glWidget->isRunning()){
        this->glWidget->avStop();
    }
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        连接成员之间的信号和槽
* @Param:        void
* @Return:       void
**/
void AVPlayer::make_connections(){

    connect(pushButton_open, &QPushButton::clicked, this, &AVPlayer::pushButton_open_clicked);
    connect(pushButton_browse, &QPushButton::clicked, this, &AVPlayer::pushButton_browse_clicked);
    connect(pushButton_back, &QPushButton::clicked, this, &AVPlayer::pushButton_back_clicked);
    connect(pushButton_advance, &QPushButton::clicked, this, &AVPlayer::pushButton_advance_clicked);
    connect(pushButton_pause, &QPushButton::clicked, this, &AVPlayer::pushButton_pause_clicked);
    connect(pushButton_restart, &QPushButton::clicked, this, &AVPlayer::pushButton_restart_clicked);

    connect(glWidget, &CppPlayer::toggleFullscreen, this, &AVPlayer::toggleFullscreen);
    connect(glWidget, &CppPlayer::needResize, this, &AVPlayer::updateGL);
    connect(glWidget, &CppPlayer::playerEnd, this, &AVPlayer::shouldLoop);

}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        打开按键槽函数，将文件路径交由CppPlayer，并开始播放
* @Param:        void
* @Return:       void
**/
void AVPlayer::pushButton_open_clicked(){
    if(this->lineEdit_path->text().isEmpty()){
        QMessageBox::information(this,"info","path is empty",QMessageBox::Ok);
        return;
    }
    if(this->glWidget->isRunning()){
        this->glWidget->avStop();
    }
    this->glWidget->setPath(this->lineEdit_path->text().toStdString());
    if(this->glWidget->avOpen()){
        this->glWidget->avStart();
    }else{
        QMessageBox::information(this,"info","can not open media",QMessageBox::Ok);
    }
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        浏览按键槽函数，打开文件选择窗口，将选择的文件路径设置到lineEdit_path
* @Param:        void
* @Return:       void
**/
void AVPlayer::pushButton_browse_clicked(){
    QString filePath(QFileDialog::getOpenFileName(nullptr,"select media","/home","all files(*.*)"));
    if(filePath.isEmpty()){
        return;
    }
    this->lineEdit_path->setText(filePath);
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        回退按键槽函数，向CppPlayer传递回退信号（默认回退5s）
* @Param:        void
* @Return:       void
**/
void AVPlayer::pushButton_back_clicked(){
    if(this->glWidget->playerCouldBeOperate()){
        this->glWidget->avBack();
    }
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        快进按键槽函数，向CppPlayer传递快进信号（默认快进5s）
* @Param:        void
* @Return:       void
**/
void AVPlayer::pushButton_advance_clicked(){
    if(this->glWidget->playerCouldBeOperate()){
        this->glWidget->avAdvance();
    }
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        暂停按键槽函数，向CppPlayer传递暂停/继续信号
* @Param:        void
* @Return:       void
**/
void AVPlayer::pushButton_pause_clicked(){
    if(!this->glWidget->avPause()){
        this->glWidget->avResume();
    }
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        重播按键槽函数，向CppPlayer传递重播信号
* @Param:        void
* @Return:       void
**/
void AVPlayer::pushButton_restart_clicked(){
    this->glWidget->avRestart();
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        label_av更新槽函数，每200ms更新当前文件播放时间
* @Param:        void
* @Return:       void
**/
void AVPlayer::label_av_update(){
    this->label_av->setText(QString("A/V: ")+QString::number(this->glWidget->getCurrentPts().first / 1000000.0f,'f',2));
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        CppPlayer播放完毕后触发的槽函数，根据复选框判断是否需要重播（即勾选会一直循环播放）
* @Param:        void
* @Return:       void
**/
void AVPlayer::shouldLoop(){
    if(this->checkBox_loop->isChecked()){
        this->glWidget->avRestart();
    }
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        全屏/退出全屏槽函数，由CppPlayer触发
* @Param:        void
* @Return:       void
**/
void AVPlayer::toggleFullscreen(bool fs){
    lineEdit_path->setVisible(!fs);
    pushButton_open->setVisible(!fs);
    pushButton_browse->setVisible(!fs);
    pushButton_back->setVisible(!fs);
    pushButton_advance->setVisible(!fs);
    pushButton_pause->setVisible(!fs);
    pushButton_restart->setVisible(!fs);
    checkBox_loop->setVisible(!fs);
    label_av->setVisible(!fs);
    if (fs) {
        vLayout_main->setStretch(1, 0);
        vLayout_main->setStretch(2, 0);
        showFullScreen();
    } else {
        vLayout_main->setStretch(1, 1);
        vLayout_main->setStretch(2, 1);
        showNormal();
    }
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        更新CppPlayer画面
* @Param:        void
* @Return:       void
**/
void AVPlayer::updateGL(){
    //因为窗口大小不改变，执行updateGL不会刷新画面
    resize(this->width(),this->height() + 1);
    resize(this->width(),this->height() - 1);
}

