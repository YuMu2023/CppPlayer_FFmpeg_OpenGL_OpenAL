/**
* @Author:       Li
* @Version:      1.0
* @Date:         2025-03-26
* @Description:  CppPlayer.h的实现
**/

#include<CppPlayer.h>
#include<QLabel>
#include<QHBoxLayout>
#include<QKeyEvent>
#include<QImage>
#include<QImageReader>
#include<QDebug>
#include<QFile>
#include<QDesktopWidget>
#include<QApplication>
#include<QOpenGLContext>
#include<QSurfaceFormat>
#include<QOpenGLFunctions>
#include<QOpenGLFunctions_3_0>
#include<QSurface>
#include<QTimer>

#include<math.h>
#include<iostream>
#include<fstream>
#include<string>
#include<cstring>

#include<AL/alc.h>
#include<AL/al.h>

extern "C"{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/pixdesc.h"
#include "libswresample/swresample.h"
#include "libavutil/avutil.h"
}

using namespace MediaUse;
using std::cout;
using std::endl;



/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        摘要
* @Param:        @parent (QWidget *) 用于设置当前窗口的parent
*                @name   (const char *) 用于设置当前窗口的标题，暂时弃用（在纯OpenGL环境下使用）
*                @fs     (bool) 用于设置全屏窗口，暂时弃用（在纯OpenGL环境下使用）
* @Return:       void
**/
CppPlayer::CppPlayer(QWidget*parent, const char* name, bool fs):
    QGLWidget(parent){

    //连接GL渲染更新的信号与槽
    connect(this,&CppPlayer::updateGLrender,this,&CppPlayer::updateGL,Qt::QueuedConnection);

    this->avInit();
    this->fullScreen = fs;
    if(fs) showFullScreen();

    this->PBO[0] = 0;
    this->PBO[1] = 0;
    this->videoTexture = 0;

    //设置强聚焦，即使嵌入其他窗口也能够按键控制，不需要可以关闭
    setFocusPolicy(Qt::StrongFocus);
    setFocus();

}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        稀构函数，释放资源
* @Param:        void
* @Return:       void
**/
CppPlayer::~CppPlayer(){
    this->avClear();
#ifdef CPPPLAYER_DEBUG
    this->log.close();
#endif
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        重写初始化OpenGL函数
* @Param:        void
* @Return:       void
**/
void CppPlayer::initializeGL(){

    //创建纹理和2个PBO缓冲
    QOpenGLFunctions* openGL_funcs = QOpenGLContext::currentContext()->functions();
    glGenTextures(1, &this->videoTexture);
    openGL_funcs->glGenBuffers(2, this->PBO);

    //获取当前上下文指针
    this->mainGLContext = QOpenGLContext::currentContext();
    this->mainSurface = this->mainGLContext->surface();

    //启用2D纹理，及相关OpenGL设置
    glEnable(GL_TEXTURE_2D);
    glShadeModel(GL_SMOOTH);
    glClearColor(0.0f,0.0f,0.0f,0.0f);
    glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        重写渲染函数，updateGL会触发该函数
* @Param:        void
* @Return:       void
**/
void CppPlayer::paintGL(){

    //清除buffer
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    //各种变换矩阵变为单位矩阵
    glLoadIdentity();

    //激活2D纹理，绑定并绘制
    glActiveTexture(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D,this->videoTexture);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f,0.0f);
    glVertex3f(-1.0f,1.0f,0.0f);
    glTexCoord2f(1.0f,0.0f);
    glVertex3f(1.0f,1.0f,0.0f);
    glTexCoord2f(1.0f,1.0f);
    glVertex3f(1.0f,-1.0f,0.0f);
    glTexCoord2f(0.0f,1.0f);
    glVertex3f(-1.0f,-1.0f,0.0f);
    glEnd();

}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        重写窗口大小变换函数
* @Param:        @width int 改变后的宽
*                @height int 改变后的高
* @Return:       void
**/
void CppPlayer::resizeGL(int width, int height){
    if(height == 0) height = 1;
    if(this->windowHeight == 0){
        glViewport(0, 0, width, height);
        return;
    }
    //保持画面比例
    float ratio = (float)this->windowWidth / this->windowHeight;
    float startW=0,startH=0;
    float tempSize = 0;
    if (((float)this->windowWidth / width) < ((float)this->windowHeight / height)) {
        tempSize = height * ratio;
        startW = (width - tempSize) / 2;
        width = tempSize;
    }
    else {
        tempSize = width / ratio;
        startH = (height - tempSize) / 2;
        height = tempSize;
    }
    glViewport(startW, startH, width, height);
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        加载或更新纹理和PBO缓冲区，一般在OpenGL第一次解出数据后触发一次
* @Param:        @openGL_funcs (QOpenGLFunctions_3_0 *) 通过子线程的共享上下文获取传来的
* @Return:       void
**/
void CppPlayer::loadGLTexture(QOpenGLFunctions_3_0* openGL_funcs){
    //解码得到的是RGB数据，*4是为了内存对齐，以免造成PBO空间不足
    int imgBufferSize = this->windowWidth * this->windowHeight * 4;
    //由于窗口大小不改变难以触发paintGL，故发出信号让父窗口刷新
    emit needResize();

    if(this->PBO[0] || this->PBO[1]){
        openGL_funcs->glDeleteBuffers(2, this->PBO);
    }
    openGL_funcs->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->PBO[0]);
    openGL_funcs->glBufferData(GL_PIXEL_UNPACK_BUFFER, imgBufferSize, 0, GL_STREAM_DRAW);
    openGL_funcs->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->PBO[1]);
    openGL_funcs->glBufferData(GL_PIXEL_UNPACK_BUFFER, imgBufferSize, 0, GL_STREAM_DRAW);

    glBindTexture(GL_TEXTURE_2D,this->videoTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, this->windowWidth, this->windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        Qt按键事件重写
* @Param:        @e (QKeyEvent *) 事件指针
* @Return:       void
**/
void CppPlayer::keyPressEvent(QKeyEvent* e){
    switch(e->key()){
    case Qt::Key_F2://F2全屏或退出全屏
        this->fullScreen = !this->fullScreen;
        if(this->fullScreen){
            //showFullScreen();
            emit toggleFullscreen(true);
        }
        else{
            //showNormal();
            emit toggleFullscreen(false);
        }
        updateGL();
        break;
    case Qt::Key_Escape://空格暂停
        if(this->playerShouldEnd){
            //close();
        }else{
            this->avStop();
        }
        break;
    case Qt::Key_Left://左键后退
        this->userOperationQueue.push(Qt::Key_Left);
        break;
    case Qt::Key_Right://右键快进
        this->userOperationQueue.push(Qt::Key_Right);
        break;
    case Qt::Key_R://R建重播
        this->userOperationQueue.push(Qt::Key_R);
        break;
    case Qt::Key_Space://Esc结束播放
        this->userOperationQueue.push(Qt::Key_Space);
        break;
    default:
        break;
    }

}

//类内静态成员初始化
bool CppPlayer::resourceInitOnce = true;
ALCdevice* CppPlayer::device = nullptr;
ALCcontext* CppPlayer::context = nullptr;
std::mutex CppPlayer::device_mutex;


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        类资源初始化，一个进程在类实例化前执行一次，不保证多进程竞争音频播放设备安全性
* @Param:        void
* @Return:       void
**/
void CppPlayer::resourceInit(){
    std::lock_guard<std::mutex> lock(device_mutex);
    if (resourceInitOnce) {
        device = nullptr;
        context = nullptr;
        resourceInitOnce = false;
    }
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        类资源释放，一个进程在不使用该类后执行一次，不保证多进程竞争音频播放设备安全性
* @Param:        void
* @Return:       void
**/
void CppPlayer::releaseResource(){
    std::lock_guard<std::mutex> lock(device_mutex);
    if (resourceInitOnce) {
        if (context) {
            alcDestroyContext(context);
            context = nullptr;
        }
        if (device) {
            alcCloseDevice(device);
            device = nullptr;
        }
        resourceInitOnce = false;
    }
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        设置文件路径，不允许在avOpen期间修改
* @Param:        @str (const std::string) 文件路径
* @Return:       void
**/
void CppPlayer::setPath(const std::string str){
    this->path = str;
    //this->setWindowTitle(str.c_str());
#ifdef CPPPLAYER_DEBUG
    if(this->log.is_open()){
        this->log.close();
    }
    std::string str2;
    for (auto i = (--str.end()); i != str.begin(); i--) {
        if (*i == '.') {
            str2 = str.substr(0, i - str.begin()) + "_log.txt";
            break;
        }
    }
    this->log.open(str2, std::ios_base::out);
#endif
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        创建子线程，开始解码和播放
* @Param:        void
* @Return:       void
**/
void CppPlayer::avStart(){
    this->ffmpegThread = new std::future<void>(std::async(std::launch::async, &CppPlayer::ffmpegReadThread, this));
    this->openGLthread = new std::future<void>(std::async(std::launch::async, &CppPlayer::openGLrenderThread, this));
    this->openALthread = new std::future<void>(std::async(std::launch::async, &CppPlayer::openALoutputThread, this));
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        阻塞的方式等待播放结束
* @Param:        void
* @Return:       void
**/
void CppPlayer::join(){
    this->ffmpegThread->wait();
    this->openGLthread->wait();
    this->openALthread->wait();
    this->avClear();
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        结束播放，需要等待线程安全结束
* @Param:        void
* @Return:       void
**/
void CppPlayer::avStop(){
    this->playerShouldEnd = true;
    this->join();
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        暂停播放，正在暂停的状态不会继续播放
* @Param:        void
* @Return:       void
**/
bool CppPlayer::avPause(){
    if(this->playerStatus.load() == CPPPLAYER_AV_PLAYING){
        this->playerStatus.store(CPPPLAYER_AV_PAUSE);
        return true;
    }
    return false;
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        继续播放，正在播放的状态不会暂停
* @Param:        void
* @Return:       void
**/
bool CppPlayer::avResume(){
    if (this->decoderStatus.load() & (CPPPLAYER_DECODER_BACK | CPPPLAYER_DECODER_ADVANCE | CPPPLAYER_DECODER_GOTO)) {
        return false;
    }
    this->playerStatus.store(CPPPLAYER_AV_PLAYING);
    return true;
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        设置单次快进和推后的时间偏移量
* @Param:        @x (std::pair<int64_t, AVRational>) <size，单位>时间偏移量
* @Return:       void
**/
void CppPlayer::setOffset(std::pair<int64_t, AVRational> x){
    this->offset = x;
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        设置当前pts，即执行一次跳转
* @Param:        @x (std::pair<int64_t, AVRational>) 需要跳转到的时间节点
* @Return:       bool 跳转设置成功返回true，并非跳转完成
**/
bool CppPlayer::setCurrentPts(std::pair<int64_t, AVRational> x){
    if (!(this->decoderStatus.load() & (CPPPLAYER_DECODER_EOF | CPPPLAYER_DECODER_ING))) {
        return false;
    }
    this->gotoPts = x;
    this->decoderStatus.store(CPPPLAYER_DECODER_GOTO);
    this->decoderStatus_cv.notify_all();
    return true;
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        单次快进
* @Param:        void
* @Return:       void
**/
void CppPlayer::avAdvance(){
    this->userOperationQueue.push(Qt::Key_Right);
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        单次后退
* @Param:        void
* @Return:       void
**/
void CppPlayer::avBack(){
    this->userOperationQueue.push(Qt::Key_Left);
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        重播
* @Param:        void
* @Return:       void
**/
bool CppPlayer::avRestart(){
    return this->setCurrentPts(std::pair<int64_t, AVRational>(0, AVRational{ 1,AV_TIME_BASE }));
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        判断现在是否在运行（解码中或播放中）
* @Param:        void
* @Return:       bool 如果三个线程没结束返回true
**/
bool CppPlayer::isRunning(){
    if(this->ffmpegThread || this->openGLthread || this->openALthread){
        return true;
    }else{
        return false;
    }
    if(this->ffmpegThread->valid() || this->openGLthread->valid() || this->openALthread->valid()){
        return true;
    }
    return false;
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        判断是否可以执行（快进/后退/跳转）操作
* @Param:        void
* @Return:       bool 如果可以执行返回true
**/
bool CppPlayer::playerCouldBeOperate(){
    if (this->decoderStatus.load() & (CPPPLAYER_DECODER_ING | CPPPLAYER_DECODER_EOF)) {
        return true;
    }
    return false;
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        返回播放状态（播放中/暂停/播放结束），详见头文件CPPPLAYER_DEFINE
* @Param:        void
* @Return:       uint8_t
**/
uint8_t CppPlayer::getPlayStatus(){
    return this->playerStatus.load();
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        返回解码状态（解码中/解码结束等），详见头文件CPPPLAYER_DEFINE
* @Param:        void
* @Return:       uint8_t
**/
uint8_t CppPlayer::getDecoderStatus(){
    return this->decoderStatus.load();
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        返回当前播放到的时间节点，即pts
* @Param:        void
* @Return:       std::pair<int64_t, AVRational>
**/
std::pair<int64_t, AVRational> CppPlayer::getCurrentPts(){
    if (this->videoStream && !this->justCover) {
        return std::pair<int64_t, AVRational>(this->videoPts.load(), AVRational{ 1,AV_TIME_BASE });
    }
    return std::pair<int64_t, AVRational>(this->audioPts.load(), AVRational{ 1,AV_TIME_BASE });
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        返回总时长
* @Param:        void
* @Return:       std::pair<int64_t, AVRational>
**/
std::pair<int64_t, AVRational> CppPlayer::getDuration(){
    return this->duration;
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        打印ffmpeg错误代码对应的字符串
* @Param:        @errEnum int ffmpeg错误代码
* @Return:       void
**/
void CppPlayer::ffmpegErrorPrint(int errEnum){
#ifdef CPPPLAYER_DEBUG
    char buf[1024] = { 0 };
    av_make_error_string(buf, sizeof(buf) - 1, errEnum);
    std::lock_guard<std::mutex> lock(this->log_mutex);
    this->log << buf << endl;
    //qDebug()<<buf;
#endif
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        输出运行信息
* @Param:        @str (const char *) 要打印的字符串
*                @color (const char *) 控制台字体颜色，详见CPPPLAYER_DEFINE
* @Return:       void
**/
void CppPlayer::messagePrint(const char* str, const char* color){
#ifdef CPPPLAYER_DEBUG
    std::lock_guard<std::mutex> lock(this->log_mutex);
    this->log << str << endl;
    //cout << color << str << CPPPLAYER_COLOR_RESET << endl;
#endif
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        对输入的视频流packet进行解码，并将得到的图像数据入队
* @Param:        @swsContext (SwsContext*&) 图像格式转换上下文
*                @packet (AVPacket*&) 视频流的一个packet
*                @frame (AVFrame*&) 临时帧指针
*                @frameDataQueue (std::queue<MediaUse::AVDataInfo>) 图像队列，解码后的图像入队于此
* @Return:       bool 成功解码一帧图像返回true
**/
bool CppPlayer::videoDecoderOneFrame(SwsContext*& swsContext, AVPacket*& packet, AVFrame*& frame, std::queue<MediaUse::AVDataInfo>& frameDataQueue){
    int ret = -1;
    unsigned char* rgb = nullptr;
    unsigned char* data[8] = { nullptr };
    int lines[8] = { 0 };
    bool successGet = false;
    this->videoIsDecoding = true;
    ret = avcodec_send_packet(this->videoCodecContext, packet);//向解码器发送packet
    av_packet_free(&packet);//释放packet资源
    packet = nullptr;
    if (ret != 0) {
        this->messagePrint("ERROR::FFMPEG::SEND_PACKET_ERROR", CPPPLAYER_COLOR_RED);
    }
    else {
        while (true) {
            ret = avcodec_receive_frame(this->videoCodecContext, frame);//循环读取帧数据
            if (ret != 0) {
                this->messagePrint("ERROR::FFMPEG::OPENGL::RECEIVE_FRAME", CPPPLAYER_COLOR_RED);
                this->ffmpegErrorPrint(ret);
                break;
            }
            this->messagePrint("INFO::FFMPEG::OPENGL::RECEIVE_A_FRAME", CPPPLAYER_COLOR_GREEN);
            if (this->windowWidth != frame->width || this->windowHeight != frame->height || !swsContext) {
                if (swsContext) {
                    sws_freeContext(swsContext);
                    swsContext = nullptr;
                }
                swsContext = sws_getCachedContext(swsContext,
                    frame->width, frame->height, (AVPixelFormat)frame->format,
                    this->windowWidth, this->windowHeight, AV_PIX_FMT_RGB24,
                    SWS_BILINEAR, nullptr, nullptr, nullptr);
                if (!swsContext) {
                    this->messagePrint("ERROR::FFMPEG::SWS_GETCACHED_CONTEXT", CPPPLAYER_COLOR_RED);
                    continue;
                }
            }
            rgb = new unsigned char[frame->width * frame->height * 4];
            if (!rgb) {
                this->messagePrint("ERROR::FFMPEG::RGB_BUFFER_ALLOC_FAILED", CPPPLAYER_COLOR_RED);
                continue;
            }
            data[0] = rgb;
            lines[0] = frame->width * 3;
            ret = sws_scale(swsContext, frame->data, frame->linesize, 0, frame->height, data, lines);//图像格式转换
            if (ret <= 0) {
                this->messagePrint("ERROR::FFMPEG::SWS_SCALE", CPPPLAYER_COLOR_RED);
                delete[] rgb;
                continue;
            }
            //得到的图像数据入队
            frameDataQueue.push(AVDataInfo(rgb, av_rescale_q(frame->pts, this->videoTimeBase, AVRational{1, AV_TIME_BASE}), 1));
            rgb = nullptr;
            successGet = true;
        }
    }
    this->videoIsDecoding = false;
    return successGet;
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        初始化各个类成员
* @Param:        void
* @Return:       void
**/
void CppPlayer::avInit(){
    this->videoShouldFlush = false;
    this->audioShouldFlush = false;
    this->videoReady = false;
    this->audioReady = false;
    this->audioIsWaiting = false;
    this->playerShouldEnd = true;
    this->justCover = false;
    this->videoIsDecoding = false;
    this->videoEnd = false;
    this->audioEnd = false;
    this->decoderStatus = CPPPLAYER_DECODER_UNKNOW;
    this->playerStatus = CPPPLAYER_AV_UNKNOW;
    this->videoPts.store(0);
    this->audioPts.store(0);
    this->videoAvgFrame = 0;
    this->audioSampleRate = 0;
    this->videoStreamIndex = -1;
    this->audioStreamIndex = -1;
    this->windowWidth = 0;
    this->windowHeight = 0;
    this->lastKey = std::pair<int, int>(0, 0);
    this->duration = std::pair<int64_t, AVRational>(0, AVRational{ 1,AV_TIME_BASE });
    this->offset = std::pair<int64_t, AVRational>(5, AVRational{ 1,1 });
    this->videoTimeBase = AVRational{ 1,AV_TIME_BASE };
    this->audioTimeBase = AVRational{ 1,AV_TIME_BASE };
    this->formatContext = nullptr;
    this->videoStream = nullptr;
    this->audioStream = nullptr;
    this->videoCodecContext = nullptr;
    this->audioCodecContext = nullptr;
    this->swrContext = nullptr;
    this->device = nullptr;
    this->context = nullptr;
    this->ffmpegThread = nullptr;
    this->openGLthread = nullptr;
    this->openALthread = nullptr;
    this->queueUseIndex = 0;
    this->queueFlushIndex = 1;
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        释放或清空资源
* @Param:        void
* @Return:       void
**/
void CppPlayer::avClear(){
    if (this->formatContext) {
        avformat_free_context(this->formatContext);
    }
    if (this->videoCodecContext) {
        avcodec_free_context(&this->videoCodecContext);
    }
    if (this->audioCodecContext) {
        avcodec_free_context(&this->audioCodecContext);
    }
    if (this->swrContext) {
        swr_free(&this->swrContext);
    }
    if (this->ffmpegThread) {
        if(this->ffmpegThread->valid()){
            this->ffmpegThread->wait();
        }
        delete this->ffmpegThread;
    }
    if (this->openGLthread) {
        if(this->openGLthread->valid()){
            this->openGLthread->wait();
        }
        delete this->openGLthread;
    }
    if (this->openALthread) {
        if(this->openALthread->valid()){
            this->openALthread->wait();
        }
        delete this->openALthread;
    }
    this->videoShouldFlush = false;
    this->audioShouldFlush = false;
    this->videoReady = false;
    this->audioReady = false;
    this->audioIsWaiting = false;
    this->playerShouldEnd = true;
    this->videoEnd = false;
    this->audioEnd = false;
    this->decoderStatus = CPPPLAYER_DECODER_UNKNOW;
    this->playerStatus = CPPPLAYER_AV_UNKNOW;
    this->videoPts.store(0);
    this->audioPts.store(0);
    this->videoAvgFrame = 0;
    this->audioSampleRate = 0;
    this->windowWidth = 0;
    this->windowHeight = 0;
    this->lastKey = std::pair<int, int>(0, 0);
    this->userOperationQueue.clear();
    this->videoTimeBase = AVRational{ 1,AV_TIME_BASE };
    this->audioTimeBase = AVRational{ 1,AV_TIME_BASE };
    this->formatContext = nullptr;
    this->videoStream = nullptr;
    this->audioStream = nullptr;
    this->videoCodecContext = nullptr;
    this->audioCodecContext = nullptr;
    this->swrContext = nullptr;
    this->device = nullptr;
    this->context = nullptr;
    this->ffmpegThread = nullptr;
    this->openGLthread = nullptr;
    this->openALthread = nullptr;
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        打开文件
* @Param:        void
* @Return:       bool 打开成功返回true
**/
bool CppPlayer::avOpen() {

    int ret = 0;
    int videoIndex = -1;
    int audioIndex = -1;
    std::string comment;
    AVDictionaryEntry* m = nullptr;
    AVChannelLayout channel_layout = AV_CHANNEL_LAYOUT_STEREO;
    const AVCodec* videoCodec = nullptr;
    const AVCodec* audioCodec = nullptr;
    this->videoStreamIndex = -1;
    this->audioStreamIndex = -1;
    AVDictionary*opts = nullptr;
    std::lock_guard<std::mutex> lock(this->device_mutex);

    this->avInit();

    //Open input media file
    av_dict_set(&opts,"stimeout","5000000",0);
    ret = avformat_open_input(&this->formatContext, path.c_str(), nullptr, &opts);
    av_dict_free(&opts);
    if (ret != 0) {
        this->messagePrint("ERROR::FFMPEG::OPEN_INPUT", CPPPLAYER_COLOR_RED);
        ffmpegErrorPrint(ret);
        this->avClear();
        return false;
    }

    //Find stream(video or audio)
    ret = avformat_find_stream_info(this->formatContext, nullptr);
    if (ret < 0) {
        this->messagePrint("ERROR::FFMPEG::FIND_STREAM_INFO", CPPPLAYER_COLOR_RED);
        ffmpegErrorPrint(ret);
        this->avClear();
        return false;
    }
    //Print media info
    av_dump_format(this->formatContext, 0, this->path.c_str(), 0);

    //Find video stream and audio stream
    videoIndex = av_find_best_stream(this->formatContext, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    audioIndex = av_find_best_stream(this->formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (videoIndex == AVERROR_STREAM_NOT_FOUND) {
        this->messagePrint("WARNNING::FFMPEG::NOT_FIND_VIDEO_STREAM", CPPPLAYER_COLOR_YELLOW);
        videoIndex = -1;
    }
    else if (videoIndex == AVERROR_DECODER_NOT_FOUND) {
        this->messagePrint("WARNNING::FFMPEG::NOT_FIND_VIDEO_STREAM_DECODER", CPPPLAYER_COLOR_YELLOW);
        videoIndex = -1;
    }
    else {
        this->videoStream = this->formatContext->streams[videoIndex];
    }
    if (audioIndex == AVERROR_STREAM_NOT_FOUND) {
        this->messagePrint("WARNNING::FFMPEG::NOT_FIND_AUDIO_STREAM", CPPPLAYER_COLOR_YELLOW);
        audioIndex = -1;
    }
    else if (audioIndex == AVERROR_DECODER_NOT_FOUND) {
        this->messagePrint("WARNNING::FFMPEG::NOT_FIND_AUDIO_STREAM_DECODER", CPPPLAYER_COLOR_YELLOW);
        audioIndex = -1;
    }
    else {
        this->audioStream = this->formatContext->streams[audioIndex];
    }
    if (videoIndex == -1 && audioIndex == -1) {
        this->avClear();
        return false;
    }

    //Open video decoder
    if (this->videoStream && videoIndex != -1) {
        videoCodec = avcodec_find_decoder(this->videoStream->codecpar->codec_id);
        if (!videoCodec) {
            this->messagePrint("ERROR::FFMPEG::CAN_NOT_FIND_VIDEO_DECODER", CPPPLAYER_COLOR_RED);
            this->videoStream = nullptr;
            videoIndex = -1;
        }
        else {
            this->videoCodecContext = avcodec_alloc_context3(videoCodec);
            if (!this->videoCodecContext) {
                this->messagePrint("ERROR::FFMPEG::AVCODEC_ALLOC_CONTEXT3", CPPPLAYER_COLOR_RED);
                this->videoStream = nullptr;
                videoIndex = -1;
            }
            else {
                ret = avcodec_parameters_to_context(this->videoCodecContext, this->videoStream->codecpar);
                if (ret < 0) {
                    this->messagePrint("ERROR::FFMPEG::CAN_NOT_COPY_PARAMETERS_TO_VIDEO_CODEC_CONTEXT", CPPPLAYER_COLOR_RED);
                    ffmpegErrorPrint(ret);
                    this->videoStream = nullptr;
                    videoIndex = -1;
                }
                else {
                    this->videoCodecContext->thread_count = 8;
                    ret = avcodec_open2(this->videoCodecContext, nullptr, nullptr);
                    if (ret != 0) {
                        this->messagePrint("ERROR::FFMPEG::CAN_NOT_OPEN_VIDEO_DECODER", CPPPLAYER_COLOR_RED);
                        this->videoStream = nullptr;
                        videoIndex = -1;
                    }
                }
            }
        }
    }

    //Open audio decoder
    if (this->audioStream && audioIndex != -1) {
        audioCodec = avcodec_find_decoder(this->audioStream->codecpar->codec_id);
        if (!audioCodec) {
            this->messagePrint("ERROR::FFMPEG::CAN_NOT_FIND_AUDIO_DECODER", CPPPLAYER_COLOR_RED);
            this->audioStream = nullptr;
            audioIndex = -1;
        }
        else {
            this->audioCodecContext = avcodec_alloc_context3(audioCodec);
            if (!this->audioCodecContext) {
                this->messagePrint("ERROR::FFMPEG::AVCODEC_ALLOC_CONTEXT3", CPPPLAYER_COLOR_RED);
                this->audioStream = nullptr;
                audioIndex = -1;
            }
            else {
                ret = avcodec_parameters_to_context(this->audioCodecContext, this->audioStream->codecpar);
                if (ret < 0) {
                    this->messagePrint("ERROR::FFMPEG::CAN_NOT_COPY_PARAMETERS_TO_AUDIO_CODEC_CONTEXT", CPPPLAYER_COLOR_RED);
                    ffmpegErrorPrint(ret);
                    this->audioStream = nullptr;
                    audioIndex = -1;
                }
                else {
                    this->audioCodecContext->thread_count = 8;
                    ret = avcodec_open2(this->audioCodecContext, nullptr, nullptr);
                    if (ret != 0) {
                        this->messagePrint("ERROR::FFMPEG::CAN_NOT_OPEN_AUDIO_DECODER", CPPPLAYER_COLOR_RED);
                        this->audioStream = nullptr;
                        audioIndex = -1;
                    }
                }
            }
        }
    }

    //Get media info
    if (this->videoStream == nullptr && this->audioStream == nullptr) {
        this->avClear();
        return false;
    }
    if (this->videoStream) {
        this->videoCodecContext->pkt_timebase = this->videoStream->time_base;
        if(this->videoStream->nb_frames > 1){
            if(this->videoStream->avg_frame_rate.num){
                this->videoAvgFrame = this->videoStream->avg_frame_rate.den ? (float)av_q2d(this->videoStream->avg_frame_rate) : 0.0f;
            }else{
                this->videoAvgFrame = this->videoCodecContext->framerate.den ? (float)av_q2d(this->videoCodecContext->framerate) : 0.0f;
            }
            if(this->videoAvgFrame == 0.0f) this->videoAvgFrame = 30.0f;
        }else{
            this->videoAvgFrame = 0.0f;
        }
        this->windowWidth = this->videoCodecContext->width;
        this->windowHeight = this->videoCodecContext->height;
    }
    if (this->audioStream) {
        this->audioCodecContext->pkt_timebase = this->audioStream->time_base;
        this->audioSampleRate = this->audioCodecContext->sample_rate;
        this->swrContext = swr_alloc();
        if (!this->swrContext) {
            this->messagePrint("ERROR::FFMPEG::SWR_ALLOC", CPPPLAYER_COLOR_RED);
            ret = -1;
        }
        else {
            ret = swr_alloc_set_opts2(&this->swrContext,
                &channel_layout,
                AV_SAMPLE_FMT_S16,
                this->audioSampleRate,
                &this->audioCodecContext->ch_layout,
                this->audioCodecContext->sample_fmt,
                this->audioSampleRate,
                0, nullptr);
            if (ret != 0) {
                this->messagePrint("ERROR::FFMPEG::SWR_SET_OPTS2", CPPPLAYER_COLOR_RED);
                ffmpegErrorPrint(ret);
            }
            ret = swr_init(this->swrContext);
            if (ret != 0) {
                this->messagePrint("ERROR::FFMPEG::SWR_INIT", CPPPLAYER_COLOR_RED);
                ffmpegErrorPrint(ret);
                ret = -1;
            }
        }
        if (ret != 0) {
            this->audioStream = nullptr;
            audioIndex = -1;
        }
    }
    if (this->videoStream == nullptr && this->audioStream == nullptr) {
        this->avClear();
        return false;
    }

    this->videoStreamIndex = videoIndex;
    this->audioStreamIndex = audioIndex;

    //判断视频流是否只有一帧
    if (this->videoStream) {
        m = av_dict_get(this->videoStream->metadata, "comment", nullptr, 0);
        if (m) {
            comment = m->value;
            comment = comment.substr(0, 5);
            if (comment == "cover" || comment == "Cover") {
                this->justCover = true;
            }
        }
        else if (this->videoAvgFrame == 0.0f) {
            this->justCover = true;
        }
    }
    this->duration.first = this->formatContext->duration;
    this->duration.second = AVRational{ 1,AV_TIME_BASE };

    return true;

}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        ffmpeg解码线程，在该线程中持续读取音视频packet，并对音频packet解码，视频packet由OpenGL线程解码，
*                快进/后退/重播/跳转操作在该线程首先执行
* @Param:        void
* @Return:       void
**/
void CppPlayer::ffmpegReadThread(){
    int out_nb_channels = 2;
    int out_pcm_buffer_size = 0;
    int ret = -1;
    bool decoderShouldEnd = false;
    int64_t nowPts = 0;
    int64_t offsetPts = 0;
    unsigned char nowStatus = CPPPLAYER_DECODER_UNKNOW;
    unsigned char* pcm = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    int seekStreamIndex = -1;

    packet = av_packet_alloc();
    if (!packet) {
        this->messagePrint("ERROR::FFMPEG::PACKET_ALLOC_FAILED", CPPPLAYER_COLOR_RED);
        this->playerShouldEnd = true;
        return;
    }
    frame = av_frame_alloc();
    if (!frame) {
        this->messagePrint("ERROR::FFMPEG::FRAME_ALLOC_FAILED", CPPPLAYER_COLOR_RED);
        this->playerShouldEnd = true;
        av_packet_free(&packet);
        return;
    }

    if (this->videoStream){
        this->videoTimeBase = this->videoStream->time_base;
    }else{
        this->videoEnd = true;
    }
    if (this->audioStream){
        this->audioTimeBase = this->audioStream->time_base;
    }
    else{
        this->audioPts.store(INT64_MAX);
        this->audioEnd = true;
    }
    this->playerShouldEnd = false;
    this->decoderStatus.store(CPPPLAYER_DECODER_ING);
    this->playerStatus.store(CPPPLAYER_AV_PLAYING);

    while (!decoderShouldEnd) {

        //每次循环读取一次解码状态
        nowStatus = this->decoderStatus.load();
        if (nowStatus == CPPPLAYER_DECODER_STOP || this->playerShouldEnd) break;
        if (nowStatus == CPPPLAYER_DECODER_EOF) {//如果读取完毕
            while(!(this->videoEnd && this->audioEnd)){//会一直等待状态改变，如快进/跳转等操作，或者音视频全都播放完毕
                if(this->decoderStatus.load() != CPPPLAYER_DECODER_EOF || this->playerShouldEnd) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if(this->videoEnd && this->audioEnd) emit this->playerEnd();//发出播放结束信号，循环播放需要外部接受信号并执行avRestart
            std::unique_lock<std::mutex> lock(this->decoderStatus_mutex);//然后一直等待直到外部手动改变状态，或结束播放
            this->decoderStatus_cv.wait(lock, [this] {return this->decoderStatus.load() != CPPPLAYER_DECODER_EOF || this->playerShouldEnd; });
            if (this->playerShouldEnd) break;
            nowStatus = this->decoderStatus.load();
            if(this->videoStream) this->videoEnd = false;
            if(this->audioStream) this->audioEnd = false;
        }
        if (nowStatus & (CPPPLAYER_DECODER_ADVANCE | CPPPLAYER_DECODER_BACK | CPPPLAYER_DECODER_GOTO)) {//如果需要跳转操作
            this->queueUseIndex.store(this->queueFlushIndex.exchange(this->queueUseIndex.load()));//更换使用队列和刷新队列下标
            this->videoShouldFlush = true;
            this->audioShouldFlush = true;
            this->playerStatus.store(CPPPLAYER_AV_PAUSE);
            offsetPts = av_rescale_q(this->offset.first, this->offset.second, AVRational{ 1,AV_TIME_BASE });
            if (nowStatus == CPPPLAYER_DECODER_BACK) offsetPts = offsetPts * (-1);
            if (this->videoStream && !this->justCover) {
                nowPts = av_rescale_q(this->videoPts.load() + offsetPts, AVRational{1,AV_TIME_BASE}, this->videoTimeBase);
                seekStreamIndex = this->videoStreamIndex;
            }
            else{
                nowPts = av_rescale_q(this->audioPts.load() + offsetPts, AVRational{1,AV_TIME_BASE}, this->audioTimeBase);
                seekStreamIndex = this->audioStreamIndex;
            }
            if (nowStatus == CPPPLAYER_DECODER_GOTO) {
                nowPts = av_rescale_q(this->gotoPts.first, this->gotoPts.second, AVRational{ 1,AV_TIME_BASE });
                seekStreamIndex = -1;
            }
            if (this->videoStream){
                while(this->videoIsDecoding);
                avcodec_flush_buffers(this->videoCodecContext);
            }
            if (this->audioStream) avcodec_flush_buffers(this->audioCodecContext);
            //根据音视频流状态设置跳转位置
            avformat_seek_file(this->formatContext, seekStreamIndex, INT64_MIN, nowPts, INT64_MAX, AVSEEK_FLAG_BACKWARD);
            //##############################################################
            if (this->audioStream) {//等待OpenAL线程进入等待状态（需要改进）
                while (!this->audioIsWaiting && !this->playerShouldEnd);
            }
            //##############################################################
            this->decoderStatus.store(CPPPLAYER_DECODER_ING);
            this->playerStatus.store(CPPPLAYER_AV_PLAYING);
        }

        ret = av_read_frame(this->formatContext, packet);//读取packet
        if (ret != 0) {
            this->messagePrint("INFO::FFMPEG::FILE_DECODER_EOF", CPPPLAYER_COLOR_RED);
            this->ffmpegErrorPrint(ret);
            this->decoderStatus.store(CPPPLAYER_DECODER_EOF);
            continue;
        }
        if (this->videoStreamIndex != -1 && packet->stream_index == this->videoStreamIndex) {
            while (this->videoPacketQueue[this->queueUseIndex.load()].size() > ((int)this->videoAvgFrame * 4)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                nowStatus = this->decoderStatus.load();
                if (nowStatus == CPPPLAYER_DECODER_STOP || this->playerShouldEnd) {
                    decoderShouldEnd = true;
                    break;
                }
                else if (nowStatus & (CPPPLAYER_DECODER_ADVANCE | CPPPLAYER_DECODER_BACK)) {
                    break;
                }
            }
            this->videoPacketQueue[this->queueUseIndex.load()].push(packet);//视频packet交由OpenGL自己解码
            packet = av_packet_alloc();
            continue;
        }
        else if (this->audioStreamIndex == -1) {
            continue;
        }

        ret = avcodec_send_packet(this->audioCodecContext, packet);
        av_packet_unref(packet);
        if (ret != 0) {
            this->messagePrint("ERROR::FFMPEG::SEND_PACKET_ERROR", CPPPLAYER_COLOR_RED);
            this->ffmpegErrorPrint(ret);
            continue;
        }
        while (true) {
            ret = avcodec_receive_frame(this->audioCodecContext, frame);
            if (ret != 0) {
                this->messagePrint("ERROR::FFMPEG::RECEIVE_FRAME", CPPPLAYER_COLOR_RED);
                ffmpegErrorPrint(ret);
                break;
            }
            this->messagePrint("INFO::FFMPEG::RECEIVE_A_FRAME", CPPPLAYER_COLOR_GREEN);

            if (!pcm) {
                pcm = new unsigned char[frame->nb_samples * 2 * 3];
                if (!pcm) {
                    this->messagePrint("ERROR::FFMPEG::PCM_BUFFER_ALLOC_FAILED", CPPPLAYER_COLOR_RED);
                    continue;
                }
            }
            ret = swr_convert(this->swrContext, &pcm, frame->nb_samples, (const uint8_t**)frame->data, frame->nb_samples);
            if (ret <= 0) {
                this->messagePrint("ERROR::FFMPEG::SWR_CONVERT", CPPPLAYER_COLOR_RED);
                delete[]pcm;
                pcm = nullptr;
                continue;
            }
            out_pcm_buffer_size = av_samples_get_buffer_size(nullptr, out_nb_channels, frame->nb_samples, AV_SAMPLE_FMT_S16, 1);
            this->audioDataQueue[this->queueUseIndex.load()].push(AVDataInfo(pcm, av_rescale_q(frame->pts, this->audioTimeBase, AVRational{1, AV_TIME_BASE}), out_pcm_buffer_size));
            pcm = nullptr;//音频packet解码后的pcm数据通过队列交由OpenAL输出
        }

    }

    if (packet) {
        av_packet_free(&packet);
    }
    if (frame) {
        av_frame_free(&frame);
    }
    if (pcm) {
        delete[] pcm;
    }
    ret = this->videoPacketQueue[0].size();
    for (int i = 0; i < ret; i++) {
        packet = this->videoPacketQueue[0].pop();
        av_packet_free(&packet);
    }
    ret = this->videoPacketQueue[1].size();
    for (int i = 0; i < ret; i++) {
        packet = this->videoPacketQueue[1].pop();
        av_packet_free(&packet);
    }
    this->audioDataQueue[0].clearWithDelete();
    this->audioDataQueue[1].clearWithDelete();

#ifdef CPPPLAYER_DEBUG
    qDebug()<<"ffmpeg end";
#endif

    this->messagePrint("INFO::FFMPEG::DECODER_END", CPPPLAYER_COLOR_GREEN);
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        OpenGL渲染线程，同时也负责用户操作的响应（即使没有视频流也能够响应），
*                该线程自己解码packet获取帧数据，视频流向音频流时间对齐
* @Param:        void
* @Return:       void
**/
void CppPlayer::openGLrenderThread(){
    int ret = -1;
    int index = 0;
    int nextIndex = 1;
    uint8_t tempIndex = 0;
    int64_t videoPBOpts[2] = { 0,0 };
    bool PBOshouldWrite[2] = { true,true };
    AVPacket* packet = nullptr;
    AVFrame* frame = av_frame_alloc();
    SwsContext* swsContext = nullptr;
    std::queue<AVDataInfo> frameDataQueue;
    int imgBufferSize = this->windowWidth * this->windowHeight * 3;
    bool shouldCheckKey = false;
    Qt::Key finalKey = Qt::Key_0;
    unsigned char tDecoderStatus = CPPPLAYER_DECODER_UNKNOW;
    unsigned char tPlayerStatus = CPPPLAYER_AV_UNKNOW;
    QOpenGLContext* sharedContext = nullptr;
    QOpenGLFunctions_3_0* openGL_funcs = nullptr;
    GLubyte* ptr = nullptr;
    std::chrono::time_point<std::chrono::system_clock> startC = std::chrono::system_clock::now();
    std::chrono::time_point<std::chrono::system_clock> nowC = std::chrono::system_clock::now();
    std::chrono::milliseconds startM;
    std::chrono::milliseconds nowM;

#ifdef CPPPLAYER_DEBUG
    std::ofstream f;
    double max = 0;
#endif

    if (!this->videoPacketQueue[this->queueUseIndex.load()].waitFor(10000) && this->videoStream) {
        goto OPENGLRENDERTHREAD_END;
    }
    if(this->audioStream){
        for (int i = 0; i < 1000; i++) {
            if (this->audioReady)break;
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!this->audioReady) {
            goto OPENGLRENDERTHREAD_END;
        }
    }
    while (true && this->videoStream) {//预先解码一帧图像
        if (this->videoPacketQueue[this->queueUseIndex.load()].empty()) {
            if (!this->videoPacketQueue[this->queueUseIndex.load()].waitFor(100)) {
                if(this->justCover){//maybe need some flush
                    ret = 10;
                    packet = nullptr;
                    while(ret){
                        if (this->videoDecoderOneFrame(swsContext, packet, frame, frameDataQueue)) {
                            ret = -1;
                            break;
                        }
                        ret--;
                    }
                    if(ret == -1) break;
                }
                goto OPENGLRENDERTHREAD_END;
            }
        }
        packet = this->videoPacketQueue[this->queueUseIndex.load()].pop();
        if (this->videoDecoderOneFrame(swsContext, packet, frame, frameDataQueue)) {
            break;
        }
    }

    //开始共享上下文
    sharedContext = new QOpenGLContext;
    sharedContext->setFormat(this->mainGLContext->format());
    sharedContext->setShareContext(this->mainGLContext);
    sharedContext->create();
    if(!sharedContext->makeCurrent(this->mainSurface)){
#ifdef CPPPLAYER_DEBUG
        qWarning("Failed to make shared OpenGL context current");
#endif
        goto OPENGLRENDERTHREAD_END;
    }

    openGL_funcs = sharedContext->versionFunctions<QOpenGLFunctions_3_0>();
    openGL_funcs->initializeOpenGLFunctions();
    this->loadGLTexture(openGL_funcs);
    openGL_funcs->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->PBO[index]);
    openGL_funcs->glBufferData(GL_PIXEL_UNPACK_BUFFER, imgBufferSize, nullptr, GL_STREAM_DRAW);
    ptr = (GLubyte*)openGL_funcs->glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    if (ptr && this->videoStream) {
        std::memcpy(ptr, frameDataQueue.front().data, imgBufferSize);
        openGL_funcs->glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
        videoPBOpts[index] = frameDataQueue.front().pts;
        frameDataQueue.front().clear();
        frameDataQueue.pop();
        PBOshouldWrite[index] = false;
        ptr = nullptr;
    }
    this->videoReady = true;

    startC = std::chrono::system_clock::now();
    nowC = std::chrono::system_clock::now();
    while(!this->playerShouldEnd){
        if(this->videoShouldFlush){
            ret = this->videoPacketQueue[this->queueFlushIndex.load()].size();
            while(ret-- > 0){
                packet = this->videoPacketQueue[this->queueFlushIndex.load()].pop();
                av_packet_free(&packet);
            }
            this->videoShouldFlush = false;
            while(!frameDataQueue.empty()){
                frameDataQueue.front().clear();
                frameDataQueue.pop();
            }
            PBOshouldWrite[index] = true;
            PBOshouldWrite[nextIndex] = true;
        }

        //两个PBO轮流传输数据给纹理
        if(PBOshouldWrite[nextIndex] && !frameDataQueue.empty()){
            openGL_funcs->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->PBO[nextIndex]);
            openGL_funcs->glBufferData(GL_PIXEL_UNPACK_BUFFER, imgBufferSize, nullptr, GL_STREAM_DRAW);
            ptr = (GLubyte*)openGL_funcs->glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
            if (ptr) {
                std::memcpy(ptr, frameDataQueue.front().data, imgBufferSize);
                openGL_funcs->glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                videoPBOpts[nextIndex] = frameDataQueue.front().pts;
                frameDataQueue.front().clear();
                frameDataQueue.pop();
                PBOshouldWrite[nextIndex] = false;
                ptr = nullptr;
            }
        }
        if (PBOshouldWrite[index] == true && PBOshouldWrite[nextIndex] == false) std::swap(index, nextIndex);
        if (!PBOshouldWrite[index] && videoPBOpts[index] <= this->audioPts.load()) {
            glBindTexture(GL_TEXTURE_2D, this->videoTexture);
            openGL_funcs->glBindBuffer(GL_PIXEL_UNPACK_BUFFER, this->PBO[index]);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            openGL_funcs->glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, this->windowWidth, this->windowHeight, GL_RGB, GL_UNSIGNED_BYTE, 0);
            glFlush();//需要立即提交操作，不等待OpenGL命令缓存区满
            this->videoPts.store(videoPBOpts[index]);
            PBOshouldWrite[index] = true;
            emit updateGLrender();
            if(!this->audioStream && !this->justCover){//如果只有视频流，则需要定时播放
                std::this_thread::sleep_for(std::chrono::milliseconds((int)(1000 / this->videoAvgFrame) - 3));
            }
        }

        //如果播放完毕后需要立即开始循环请打开
//        if(frameDataQueue.empty() && this->videoPacketQueue[tempIndex].empty() && this->decoderStatus.load() == CPPPLAYER_DECODER_EOF){
//            this->videoEnd = true;
//        }

        tempIndex = this->queueUseIndex.load();
        if (!this->videoPacketQueue[tempIndex].empty() && frameDataQueue.size() < 5) {//预存5帧画面，保持流畅性和低内存消耗，帧数过高(帧间隔+传输时间<=解码时间)可能造成卡顿
            while (true) {
                if (this->videoPacketQueue[tempIndex].empty()) {
                    if (this->videoShouldFlush || this->playerShouldEnd || this->decoderStatus.load() == CPPPLAYER_DECODER_EOF)break;
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                else {
                    packet = this->videoPacketQueue[tempIndex].pop();
                    if (this->videoDecoderOneFrame(swsContext, packet, frame, frameDataQueue)) {
                        break;
                    }
                }
            }
        }else{
            if(!shouldCheckKey){
                startC = std::chrono::system_clock::now();
                startM = std::chrono::duration_cast<std::chrono::milliseconds>(startC.time_since_epoch());
                shouldCheckKey = true;
#ifdef CPPPLAYER_DEBUG
            max = (this->audioPts - this->videoPts) / 1000000.0f > max ? (this->audioPts - this->videoPts) / 1000000.0f : max;
            cout << '\r' << "A-V: " << (this->audioPts - this->videoPts) / 1000000.0f << "   " << max;
#endif
            }else{
                //100ms检查一次是否播放完毕，并不会造成多大的延迟
                if(frameDataQueue.empty() && this->videoPacketQueue[tempIndex].empty() && this->decoderStatus.load() == CPPPLAYER_DECODER_EOF){
                    this->videoEnd = true;
                }
                nowC = std::chrono::system_clock::now();
                nowM = std::chrono::duration_cast<std::chrono::milliseconds>(nowC.time_since_epoch());
                if (nowM.count() - startM.count() >= 100) {
                    if (!this->userOperationQueue.empty()) {
                        finalKey = this->userOperationQueue.back();
                        this->userOperationQueue.clear();
                        tDecoderStatus = this->decoderStatus.load();
                        tPlayerStatus = this->playerStatus.load();
                        if(finalKey == Qt::Key_Space){
                            if(tPlayerStatus == CPPPLAYER_AV_PLAYING){
                                this->playerStatus.store(CPPPLAYER_AV_PAUSE);
                            }else if(tPlayerStatus == CPPPLAYER_AV_PAUSE){
                                this->playerStatus.store(CPPPLAYER_AV_PLAYING);
                            }
                        }else if(finalKey == Qt::Key_Left && !(tDecoderStatus & CPPPLAYER_DECODER_BUSY)){
                            this->decoderStatus.store(CPPPLAYER_DECODER_BACK);
                        }else if(finalKey == Qt::Key_Right && !(tDecoderStatus & CPPPLAYER_DECODER_BUSY)){
                            this->decoderStatus.store(CPPPLAYER_DECODER_ADVANCE);
                        }else if(finalKey == Qt::Key_R && !(tDecoderStatus & CPPPLAYER_DECODER_BUSY)){
                            this->gotoPts.first = 0;
                            this->decoderStatus.store(CPPPLAYER_DECODER_GOTO);
                        }
                        this->decoderStatus_cv.notify_all();
                    }
                    shouldCheckKey = false;
                }
            }
        }
    }

OPENGLRENDERTHREAD_END:
    this->playerShouldEnd = true;
    if(sharedContext){
        sharedContext->doneCurrent();
        delete sharedContext;
    }
    this->playerStatus.store(CPPPLAYER_AV_STOP);
    this->audioDataQueue[this->queueUseIndex.load()].notify_all();
    this->decoderStatus_cv.notify_all();
    if (packet) {
        av_packet_free(&packet);
    }
    if (frame) {
        av_frame_free(&frame);
    }
    if (swsContext) {
        sws_freeContext(swsContext);
    }
    while (!frameDataQueue.empty()) {
        frameDataQueue.front().clear();
        frameDataQueue.pop();
    }

#ifdef CPPPLAYER_DEBUG
    qDebug()<<"opengl end";
#endif

    this->messagePrint("INFO::OPENGL::RENDER_END", CPPPLAYER_COLOR_GREEN);
}


/**
* @Author:       Li
* @Date:         2025-03-26
* @Version:      1.0
* @Brief:        OpenAL输出线程，只要没有其他操作，会一直根据采样率持续输出音频到设备
*                如果暂停，那么audioPlayingQueue将不会更新，视频渲染线程将一直获取到静止的pts而不更新画面，从而达到音视频都暂停的效果
* @Param:        void
* @Return:       void
**/
void CppPlayer::openALoutputThread(){
    int SBD_size = 8;
    unsigned int SSD = 0;
    unsigned int SBD[SBD_size] = { 0 };
    float sourcePos[3] = { 0.0f,0.0f,0.0f };
    float sourceVel[3] = { 0.0f,0.0f,0.0f };
    int ret = -1;
    unsigned int unQueueBufferId = 0;
    unsigned char nowStatus = CPPPLAYER_AV_UNKNOW;
    bool audioShortBuffer = false;
    uint8_t tempIndex = 0;
    AVDataInfo frame;

    if(!this->audioStream) return;
    if (!this->audioDataQueue[this->queueUseIndex.load()].waitFor(10000)) {
        this->playerShouldEnd = true;
        return;
    }

    std::unique_lock<std::mutex> device_lock(this->device_mutex);
    if (!this->device) {
        this->device = alcOpenDevice(nullptr);
        if (!this->device) {
            this->messagePrint("ERROR::OPENAL::NOT_DEVICE_USE", CPPPLAYER_COLOR_RED);
            this->playerShouldEnd = true;
            return;
        }
    }
    if (!this->context) {
        this->context = alcCreateContext(this->device, nullptr);
        if (!this->context) {
            this->messagePrint("ERROR::OPENAL::CAN_NOT_CREATE_CONTEXT", CPPPLAYER_COLOR_RED);
            this->playerShouldEnd = true;
            return;
        }
    }
    alcMakeContextCurrent(this->context);
    alGenBuffers(8, SBD);
    alGenSources(1, &SSD);
    alSourcef(SSD, AL_PITCH, 1.0f);
    alSourcef(SSD, AL_GAIN, 1.0f);
    alSourcefv(SSD, AL_POSITION, sourcePos);
    alSourcefv(SSD, AL_VELOCITY, sourceVel);
    alSourcei(SSD, AL_LOOPING, AL_FALSE);
    device_lock.unlock();

    ret = 100;
    while(ret && this->audioDataQueue[this->queueUseIndex.load()].size() < SBD_size){
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ret--;
    }
    if(ret == 0){
        SBD_size = this->audioDataQueue[this->queueUseIndex.load()].size();
    }
    ret = -1;
    this->audioPlayingQueue.setCapacity(SBD_size);
    for (int i = 0; i < SBD_size; i++) {
        frame = this->audioDataQueue[this->queueUseIndex.load()].pop();
        alBufferData(SBD[i], AL_FORMAT_STEREO16, frame.data, frame.size, this->audioSampleRate);
        this->audioPlayingQueue.push(frame.pts);
        delete[] frame.data;
    }
    this->audioPts.store(this->audioPlayingQueue.front());
    alSourceQueueBuffers(SSD, SBD_size, SBD);
    this->audioReady = true;
    while (!this->videoReady && !this->playerShouldEnd)std::this_thread::sleep_for(std::chrono::milliseconds(5));
    alSourcePlay(SSD);

    while (!this->playerShouldEnd) {
        nowStatus = this->playerStatus.load();
        if (nowStatus != CPPPLAYER_AV_PLAYING) {
            this->audioIsWaiting = true;
            alSourcePause(SSD);
            do {
                if (this->audioShouldFlush) {//跳转操作时，刷新当前帧队列，并更换到另一个帧队列
                    alSourceStop(SSD);
                    this->audioDataQueue[this->queueFlushIndex.load()].clearWithDelete();
                    ret = this->audioPlayingQueue.size();
                    while (ret-- > 0) {
                        this->audioPlayingQueue.pop();
                    }
                    this->audioShouldFlush = false;
                    audioShortBuffer = true;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                nowStatus = this->playerStatus.load();
            } while ((nowStatus != CPPPLAYER_AV_PLAYING && nowStatus != CPPPLAYER_AV_STOP) || this->audioShouldFlush);
            if (audioShortBuffer) {
                ret = 2 % SBD_size;//跳转时不需要等待全部缓冲区填满，先填充2个缓冲更新音频pts，视频得以渲染，降低跳转延迟
                while (ret-- > 0) {
                    if (this->audioDataQueue[this->queueUseIndex.load()].empty()) {
                        if (this->playerStatus.load() != CPPPLAYER_AV_PLAYING || this->playerShouldEnd || this->audioShouldFlush) {
                            break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                    else {
                        alSourceUnqueueBuffers(SSD, 1, &unQueueBufferId);
                        frame = this->audioDataQueue[this->queueUseIndex.load()].pop();
                        alBufferData(unQueueBufferId, AL_FORMAT_STEREO16, frame.data, frame.size, this->audioSampleRate);
                        alSourceQueueBuffers(SSD, 1, &unQueueBufferId);
                        delete[] frame.data;
                        this->audioPlayingQueue.push(frame.pts);
                    }
                }
                this->audioPts.store(this->audioPlayingQueue.front());
                audioShortBuffer = false;
            }
            this->audioIsWaiting = false;
            alSourcePlay(SSD);
        }

        tempIndex = this->queueUseIndex.load();
        alGetSourcei(SSD, AL_BUFFERS_PROCESSED, &ret);
        while (ret > 0) {
            if (this->audioDataQueue[tempIndex].empty()) {
                if(this->decoderStatus.load() == CPPPLAYER_DECODER_EOF){
                    this->audioEnd = true;
                    break;
                }
                if (this->playerStatus.load() != CPPPLAYER_AV_PLAYING || this->playerShouldEnd || this->audioShouldFlush) {
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            else {
                alSourceUnqueueBuffers(SSD, 1, &unQueueBufferId);
                frame = audioDataQueue[tempIndex].pop();
                alBufferData(unQueueBufferId, AL_FORMAT_STEREO16, frame.data, frame.size, this->audioSampleRate);
                alSourceQueueBuffers(SSD, 1, &unQueueBufferId);
                delete[] frame.data;
                if (this->audioPlayingQueue.size() != 0) this->audioPlayingQueue.pop();
                this->audioPlayingQueue.push(frame.pts);
                ret -= 1;
            }
        }

        alGetSourcei(SSD, AL_SOURCE_STATE, &ret);
        if (ret != AL_PLAYING) {
            alSourcePlay(SSD);
        }
        this->audioPts.store(this->audioPlayingQueue.front());
    }

    alSourceStop(SSD);
    alSourceUnqueueBuffers(SSD, SBD_size, SBD);
    alDeleteSources(1, &SSD);
    alDeleteBuffers(SBD_size, SBD);

    this->messagePrint("INFO::OPENAL::OUTPUT_END", CPPPLAYER_COLOR_GREEN);
}

