#include<MainWindow.h>
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

MainWindow::MainWindow(QWidget*parent, const char* name, bool fs):
    QGLWidget(parent){

    connect(this,&MainWindow::updateGLrender,this,&MainWindow::updateGL,Qt::QueuedConnection);

    this->avInit();
    this->fullScreen = fs;
    if(fs) showFullScreen();

    this->PBO[0] = 0;
    this->PBO[1] = 0;
    this->videoTexture = 0;

    setFocusPolicy(Qt::StrongFocus);
    setFocus();

}

MainWindow::~MainWindow(){
    this->avClear();
#ifdef AVPLAYER_DEBUG
    this->log.close();
#endif
}

void MainWindow::initializeGL(){

    QOpenGLFunctions* openGL_funcs = QOpenGLContext::currentContext()->functions();
    glGenTextures(1, &this->videoTexture);
    openGL_funcs->glGenBuffers(2, this->PBO);

    this->mainGLContext = QOpenGLContext::currentContext();
    this->mainSurface = this->mainGLContext->surface();

    glEnable(GL_TEXTURE_2D);
    glShadeModel(GL_SMOOTH);
    glClearColor(0.0f,0.0f,0.0f,0.0f);
    glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );
}

void MainWindow::paintGL(){

    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glLoadIdentity();

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

void MainWindow::resizeGL(int width, int height){
    if(height == 0) height = 1;
    if(this->windowHeight == 0){
        glViewport(0, 0, width, height);
        return;
    }
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

void MainWindow::loadGLTexture(QOpenGLFunctions_3_0* openGL_funcs){
    int imgBufferSize = this->windowWidth * this->windowHeight * 4;
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

void MainWindow::keyPressEvent(QKeyEvent* e){
    switch(e->key()){
    case Qt::Key_F2:
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
    case Qt::Key_Escape:
        if(this->playerShouldEnd){
            //close();
        }else{
            this->playerShouldEnd = true;
        }
        break;
    case Qt::Key_Left:
        this->userOperationQueue.push(Qt::Key_Left);
        break;
    case Qt::Key_Right:
        this->userOperationQueue.push(Qt::Key_Right);
        break;
    case Qt::Key_R:
        this->userOperationQueue.push(Qt::Key_R);
        break;
    case Qt::Key_Space:
        this->userOperationQueue.push(Qt::Key_Space);
        break;
    default:
        break;
    }

}

bool MainWindow::resourceInitOnce = true;
ALCdevice* MainWindow::device = nullptr;
ALCcontext* MainWindow::context = nullptr;
std::mutex MainWindow::device_mutex;

void MainWindow::resourceInit(){
    std::lock_guard<std::mutex> lock(device_mutex);
    if (resourceInitOnce) {
        device = nullptr;
        context = nullptr;
        resourceInitOnce = false;
    }
}

void MainWindow::releaseResource(){
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

void MainWindow::setPath(const std::string str){
    this->path = str;
    //this->setWindowTitle(str.c_str());
#ifdef AVPLAYER_DEBUG
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

void MainWindow::avStart(){
    this->ffmpegThread = new std::future<void>(std::async(std::launch::async, &MainWindow::ffmpegReadThread, this));
    this->openGLthread = new std::future<void>(std::async(std::launch::async, &MainWindow::openGLrenderThread, this));
    this->openALthread = new std::future<void>(std::async(std::launch::async, &MainWindow::openALoutputThread, this));
}

void MainWindow::join(){
    this->ffmpegThread->wait();
    this->openGLthread->wait();
    this->openALthread->wait();
    this->avClear();
}

void MainWindow::avStop(){
    this->playerShouldEnd = true;
    this->join();
}

bool MainWindow::avPause(){
    if(this->playerStatus.load() == AVPLAYER_AV_PLAYING){
        this->playerStatus.store(AVPLAYER_AV_PAUSE);
        return true;
    }
    return false;
}

bool MainWindow::avResume(){
    if (this->decoderStatus.load() & (AVPLAYER_DECODER_BACK | AVPLAYER_DECODER_ADVANCE | AVPLAYER_DECODER_GOTO)) {
        return false;
    }
    this->playerStatus.store(AVPLAYER_AV_PLAYING);
    return true;
}

void MainWindow::setOffset(std::pair<int64_t, AVRational> x){
    this->offset = x;
}

bool MainWindow::setCurrentPts(std::pair<int64_t, AVRational> x){
    if (!(this->decoderStatus.load() & (AVPLAYER_DECODER_EOF | AVPLAYER_DECODER_ING))) {
        return false;
    }
    this->gotoPts = x;
    this->decoderStatus.store(AVPLAYER_DECODER_GOTO);
    this->decoderStatus_cv.notify_all();
    return true;
}

void MainWindow::avAdvance(){
    this->userOperationQueue.push(Qt::Key_Right);
}

void MainWindow::avBack(){
    this->userOperationQueue.push(Qt::Key_Left);
}

bool MainWindow::avRestart(){
    return this->setCurrentPts(std::pair<int64_t, AVRational>(0, AVRational{ 1,AV_TIME_BASE }));
}

bool MainWindow::isRunning(){
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

bool MainWindow::playerCouldBeOperate(){
    if (this->decoderStatus.load() & (AVPLAYER_DECODER_ING | AVPLAYER_DECODER_EOF)) {
        return true;
    }
    return false;
}

uint8_t MainWindow::getPlayStatus(){
    return this->playerStatus.load();
}

uint8_t MainWindow::getDecoderStatus(){
    return this->decoderStatus.load();
}

std::pair<int64_t, AVRational> MainWindow::getCurrentPts(){
    if (this->videoStream && !this->justCover) {
        return std::pair<int64_t, AVRational>(this->videoPts.load(), AVRational{ 1,AV_TIME_BASE });
    }
    return std::pair<int64_t, AVRational>(this->audioPts.load(), AVRational{ 1,AV_TIME_BASE });
}

std::pair<int64_t, AVRational> MainWindow::getDuration(){
    return this->duration;
}

void MainWindow::ffmpegErrorPrint(int errEnum){
#ifdef AVPLAYER_DEBUG
    char buf[1024] = { 0 };
    av_make_error_string(buf, sizeof(buf) - 1, errEnum);
    std::lock_guard<std::mutex> lock(this->log_mutex);
    this->log << buf << endl;
    //qDebug()<<buf;
#endif
}

void MainWindow::messagePrint(const char* str, const char* color){
#ifdef AVPLAYER_DEBUG
    std::lock_guard<std::mutex> lock(this->log_mutex);
    this->log << str << endl;
    //cout << color << str << AVPLAYER_COLOR_RESET << endl;
#endif
}

bool MainWindow::videoDecoderOneFrame(SwsContext*& swsContext, AVPacket*& packet, AVFrame*& frame, std::queue<MediaUse::AVDataInfo>& frameDataQueue){
    int ret = -1;
    unsigned char* rgb = nullptr;
    unsigned char* data[8] = { nullptr };
    int lines[8] = { 0 };
    bool successGet = false;
    this->videoIsDecoding = true;
    ret = avcodec_send_packet(this->videoCodecContext, packet);
    av_packet_free(&packet);
    packet = nullptr;
    if (ret != 0) {
        this->messagePrint("ERROR::FFMPEG::SEND_PACKET_ERROR", AVPLAYER_COLOR_RED);
    }
    else {
        while (true) {
            ret = avcodec_receive_frame(this->videoCodecContext, frame);
            if (ret != 0) {
                this->messagePrint("ERROR::FFMPEG::OPENGL::RECEIVE_FRAME", AVPLAYER_COLOR_RED);
                this->ffmpegErrorPrint(ret);
                break;
            }
            this->messagePrint("INFO::FFMPEG::OPENGL::RECEIVE_A_FRAME", AVPLAYER_COLOR_GREEN);
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
                    this->messagePrint("ERROR::FFMPEG::SWS_GETCACHED_CONTEXT", AVPLAYER_COLOR_RED);
                    continue;
                }
            }
            rgb = new unsigned char[frame->width * frame->height * 4];
            if (!rgb) {
                this->messagePrint("ERROR::FFMPEG::RGB_BUFFER_ALLOC_FAILED", AVPLAYER_COLOR_RED);
                continue;
            }
            data[0] = rgb;
            lines[0] = frame->width * 3;
            ret = sws_scale(swsContext, frame->data, frame->linesize, 0, frame->height, data, lines);
            if (ret <= 0) {
                this->messagePrint("ERROR::FFMPEG::SWS_SCALE", AVPLAYER_COLOR_RED);
                delete[] rgb;
                continue;
            }
            frameDataQueue.push(AVDataInfo(rgb, av_rescale_q(frame->pts, this->videoTimeBase, AVRational{1, AV_TIME_BASE}), 1));
            rgb = nullptr;
            successGet = true;
        }
    }
    this->videoIsDecoding = false;
    return successGet;
}

void MainWindow::avInit(){
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
    this->decoderStatus = AVPLAYER_DECODER_UNKNOW;
    this->playerStatus = AVPLAYER_AV_UNKNOW;
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

void MainWindow::avClear(){
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
    this->decoderStatus = AVPLAYER_DECODER_UNKNOW;
    this->playerStatus = AVPLAYER_AV_UNKNOW;
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

bool MainWindow::avOpen() {

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
        this->messagePrint("ERROR::FFMPEG::OPEN_INPUT", AVPLAYER_COLOR_RED);
        ffmpegErrorPrint(ret);
        this->avClear();
        return false;
    }

    //Find stream(video or audio)
    ret = avformat_find_stream_info(this->formatContext, nullptr);
    if (ret < 0) {
        this->messagePrint("ERROR::FFMPEG::FIND_STREAM_INFO", AVPLAYER_COLOR_RED);
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
        this->messagePrint("WARNNING::FFMPEG::NOT_FIND_VIDEO_STREAM", AVPLAYER_COLOR_YELLOW);
        videoIndex = -1;
    }
    else if (videoIndex == AVERROR_DECODER_NOT_FOUND) {
        this->messagePrint("WARNNING::FFMPEG::NOT_FIND_VIDEO_STREAM_DECODER", AVPLAYER_COLOR_YELLOW);
        videoIndex = -1;
    }
    else {
        this->videoStream = this->formatContext->streams[videoIndex];
    }
    if (audioIndex == AVERROR_STREAM_NOT_FOUND) {
        this->messagePrint("WARNNING::FFMPEG::NOT_FIND_AUDIO_STREAM", AVPLAYER_COLOR_YELLOW);
        audioIndex = -1;
    }
    else if (audioIndex == AVERROR_DECODER_NOT_FOUND) {
        this->messagePrint("WARNNING::FFMPEG::NOT_FIND_AUDIO_STREAM_DECODER", AVPLAYER_COLOR_YELLOW);
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
            this->messagePrint("ERROR::FFMPEG::CAN_NOT_FIND_VIDEO_DECODER", AVPLAYER_COLOR_RED);
            this->videoStream = nullptr;
            videoIndex = -1;
        }
        else {
            this->videoCodecContext = avcodec_alloc_context3(videoCodec);
            if (!this->videoCodecContext) {
                this->messagePrint("ERROR::FFMPEG::AVCODEC_ALLOC_CONTEXT3", AVPLAYER_COLOR_RED);
                this->videoStream = nullptr;
                videoIndex = -1;
            }
            else {
                ret = avcodec_parameters_to_context(this->videoCodecContext, this->videoStream->codecpar);
                if (ret < 0) {
                    this->messagePrint("ERROR::FFMPEG::CAN_NOT_COPY_PARAMETERS_TO_VIDEO_CODEC_CONTEXT", AVPLAYER_COLOR_RED);
                    ffmpegErrorPrint(ret);
                    this->videoStream = nullptr;
                    videoIndex = -1;
                }
                else {
                    this->videoCodecContext->thread_count = 8;
                    ret = avcodec_open2(this->videoCodecContext, nullptr, nullptr);
                    if (ret != 0) {
                        this->messagePrint("ERROR::FFMPEG::CAN_NOT_OPEN_VIDEO_DECODER", AVPLAYER_COLOR_RED);
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
            this->messagePrint("ERROR::FFMPEG::CAN_NOT_FIND_AUDIO_DECODER", AVPLAYER_COLOR_RED);
            this->audioStream = nullptr;
            audioIndex = -1;
        }
        else {
            this->audioCodecContext = avcodec_alloc_context3(audioCodec);
            if (!this->audioCodecContext) {
                this->messagePrint("ERROR::FFMPEG::AVCODEC_ALLOC_CONTEXT3", AVPLAYER_COLOR_RED);
                this->audioStream = nullptr;
                audioIndex = -1;
            }
            else {
                ret = avcodec_parameters_to_context(this->audioCodecContext, this->audioStream->codecpar);
                if (ret < 0) {
                    this->messagePrint("ERROR::FFMPEG::CAN_NOT_COPY_PARAMETERS_TO_AUDIO_CODEC_CONTEXT", AVPLAYER_COLOR_RED);
                    ffmpegErrorPrint(ret);
                    this->audioStream = nullptr;
                    audioIndex = -1;
                }
                else {
                    this->audioCodecContext->thread_count = 8;
                    ret = avcodec_open2(this->audioCodecContext, nullptr, nullptr);
                    if (ret != 0) {
                        this->messagePrint("ERROR::FFMPEG::CAN_NOT_OPEN_AUDIO_DECODER", AVPLAYER_COLOR_RED);
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
            this->messagePrint("ERROR::FFMPEG::SWR_ALLOC", AVPLAYER_COLOR_RED);
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
                this->messagePrint("ERROR::FFMPEG::SWR_SET_OPTS2", AVPLAYER_COLOR_RED);
                ffmpegErrorPrint(ret);
            }
            ret = swr_init(this->swrContext);
            if (ret != 0) {
                this->messagePrint("ERROR::FFMPEG::SWR_INIT", AVPLAYER_COLOR_RED);
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

void MainWindow::ffmpegReadThread(){
    int out_nb_channels = 2;
    int out_pcm_buffer_size = 0;
    int ret = -1;
    bool decoderShouldEnd = false;
    int64_t nowPts = 0;
    int64_t offsetPts = 0;
    unsigned char nowStatus = AVPLAYER_DECODER_UNKNOW;
    unsigned char* pcm = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    int seekStreamIndex = -1;

    packet = av_packet_alloc();
    if (!packet) {
        this->messagePrint("ERROR::FFMPEG::PACKET_ALLOC_FAILED", AVPLAYER_COLOR_RED);
        this->playerShouldEnd = true;
        return;
    }
    frame = av_frame_alloc();
    if (!frame) {
        this->messagePrint("ERROR::FFMPEG::FRAME_ALLOC_FAILED", AVPLAYER_COLOR_RED);
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
    this->decoderStatus.store(AVPLAYER_DECODER_ING);
    this->playerStatus.store(AVPLAYER_AV_PLAYING);

    while (!decoderShouldEnd) {

        nowStatus = this->decoderStatus.load();
        if (nowStatus == AVPLAYER_DECODER_STOP || this->playerShouldEnd) break;
        if (nowStatus == AVPLAYER_DECODER_EOF) {
            while(!(this->videoEnd && this->audioEnd)){
                if(this->decoderStatus.load() != AVPLAYER_DECODER_EOF || this->playerShouldEnd) break;
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if(this->videoEnd && this->audioEnd) emit this->playerEnd();
            std::unique_lock<std::mutex> lock(this->decoderStatus_mutex);
            this->decoderStatus_cv.wait(lock, [this] {return this->decoderStatus.load() != AVPLAYER_DECODER_EOF || this->playerShouldEnd; });
            if (this->playerShouldEnd) break;
            nowStatus = this->decoderStatus.load();
            if(this->videoStream) this->videoEnd = false;
            if(this->audioStream) this->audioEnd = false;
        }
        if (nowStatus & (AVPLAYER_DECODER_ADVANCE | AVPLAYER_DECODER_BACK | AVPLAYER_DECODER_GOTO)) {
            this->queueUseIndex.store(this->queueFlushIndex.exchange(this->queueUseIndex.load()));
            this->videoShouldFlush = true;
            this->audioShouldFlush = true;
            this->playerStatus.store(AVPLAYER_AV_PAUSE);
            offsetPts = av_rescale_q(this->offset.first, this->offset.second, AVRational{ 1,AV_TIME_BASE });
            if (nowStatus == AVPLAYER_DECODER_BACK) offsetPts = offsetPts * (-1);
            if (this->videoStream && !this->justCover) {
                nowPts = av_rescale_q(this->videoPts.load() + offsetPts, AVRational{1,AV_TIME_BASE}, this->videoTimeBase);
                seekStreamIndex = this->videoStreamIndex;
            }
            else{
                nowPts = av_rescale_q(this->audioPts.load() + offsetPts, AVRational{1,AV_TIME_BASE}, this->audioTimeBase);
                seekStreamIndex = this->audioStreamIndex;
            }
            if (nowStatus == AVPLAYER_DECODER_GOTO) {
                nowPts = av_rescale_q(this->gotoPts.first, this->gotoPts.second, AVRational{ 1,AV_TIME_BASE });
                seekStreamIndex = -1;
            }
            if (this->videoStream){
                while(this->videoIsDecoding);
                avcodec_flush_buffers(this->videoCodecContext);
            }
            if (this->audioStream) avcodec_flush_buffers(this->audioCodecContext);
            avformat_seek_file(this->formatContext, seekStreamIndex, INT64_MIN, nowPts, INT64_MAX, AVSEEK_FLAG_BACKWARD);
            if (this->audioStream) {
                while (!this->audioIsWaiting && !this->playerShouldEnd);
            }
            this->decoderStatus.store(AVPLAYER_DECODER_ING);
            this->playerStatus.store(AVPLAYER_AV_PLAYING);
        }

        ret = av_read_frame(this->formatContext, packet);
        if (ret != 0) {
            this->messagePrint("INFO::FFMPEG::FILE_DECODER_EOF", AVPLAYER_COLOR_RED);
            this->ffmpegErrorPrint(ret);
            this->decoderStatus.store(AVPLAYER_DECODER_EOF);
            continue;
        }
        if (this->videoStreamIndex != -1 && packet->stream_index == this->videoStreamIndex) {
            while (this->videoPacketQueue[this->queueUseIndex.load()].size() > ((int)this->videoAvgFrame * 4)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                nowStatus = this->decoderStatus.load();
                if (nowStatus == AVPLAYER_DECODER_STOP || this->playerShouldEnd) {
                    decoderShouldEnd = true;
                    break;
                }
                else if (nowStatus & (AVPLAYER_DECODER_ADVANCE | AVPLAYER_DECODER_BACK)) {
                    break;
                }
            }
            this->videoPacketQueue[this->queueUseIndex.load()].push(packet);
            packet = av_packet_alloc();
            continue;
        }
        else if (this->audioStreamIndex == -1) {
            continue;
        }

        ret = avcodec_send_packet(this->audioCodecContext, packet);
        av_packet_unref(packet);
        if (ret != 0) {
            this->messagePrint("ERROR::FFMPEG::SEND_PACKET_ERROR", AVPLAYER_COLOR_RED);
            this->ffmpegErrorPrint(ret);
            continue;
        }
        while (true) {
            ret = avcodec_receive_frame(this->audioCodecContext, frame);
            if (ret != 0) {
                this->messagePrint("ERROR::FFMPEG::RECEIVE_FRAME", AVPLAYER_COLOR_RED);
                ffmpegErrorPrint(ret);
                break;
            }
            this->messagePrint("INFO::FFMPEG::RECEIVE_A_FRAME", AVPLAYER_COLOR_GREEN);

            if (!pcm) {
                pcm = new unsigned char[frame->nb_samples * 2 * 3];
                if (!pcm) {
                    this->messagePrint("ERROR::FFMPEG::PCM_BUFFER_ALLOC_FAILED", AVPLAYER_COLOR_RED);
                    continue;
                }
            }
            ret = swr_convert(this->swrContext, &pcm, frame->nb_samples, (const uint8_t**)frame->data, frame->nb_samples);
            if (ret <= 0) {
                this->messagePrint("ERROR::FFMPEG::SWR_CONVERT", AVPLAYER_COLOR_RED);
                delete[]pcm;
                pcm = nullptr;
                continue;
            }
            out_pcm_buffer_size = av_samples_get_buffer_size(nullptr, out_nb_channels, frame->nb_samples, AV_SAMPLE_FMT_S16, 1);
            this->audioDataQueue[this->queueUseIndex.load()].push(AVDataInfo(pcm, av_rescale_q(frame->pts, this->audioTimeBase, AVRational{1, AV_TIME_BASE}), out_pcm_buffer_size));
            pcm = nullptr;
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

#ifdef AVPLAYER_DEBUG
    qDebug()<<"ffmpeg end";
#endif

    this->messagePrint("INFO::FFMPEG::DECODER_END", AVPLAYER_COLOR_GREEN);
}

void MainWindow::openGLrenderThread(){
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
    unsigned char tDecoderStatus = AVPLAYER_DECODER_UNKNOW;
    unsigned char tPlayerStatus = AVPLAYER_AV_UNKNOW;
    QOpenGLContext* sharedContext = nullptr;
    QOpenGLFunctions_3_0* openGL_funcs = nullptr;
    GLubyte* ptr = nullptr;
    std::chrono::time_point<std::chrono::system_clock> startC = std::chrono::system_clock::now();
    std::chrono::time_point<std::chrono::system_clock> nowC = std::chrono::system_clock::now();
    std::chrono::milliseconds startM;
    std::chrono::milliseconds nowM;

#ifdef AVPLAYER_DEBUG
    std::ofstream f;
    double max = 0;
#endif

    if (!this->videoPacketQueue[this->queueUseIndex.load()].waitFor(10000)) {
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
    while (true) {
        if (this->videoPacketQueue[this->queueUseIndex.load()].empty()) {
            if (!this->videoPacketQueue[this->queueUseIndex.load()].waitFor(10000)) {
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

    sharedContext = new QOpenGLContext;
    sharedContext->setFormat(this->mainGLContext->format());
    sharedContext->setShareContext(this->mainGLContext);
    sharedContext->create();
    if(!sharedContext->makeCurrent(this->mainSurface)){
#ifdef AVPLAYER_DEBUG
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
    if (ptr) {
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
            glFlush();
            this->videoPts.store(videoPBOpts[index]);
            PBOshouldWrite[index] = true;
            emit updateGLrender();
            if(!this->audioStream && !this->justCover){
                std::this_thread::sleep_for(std::chrono::milliseconds((int)(1000 / this->videoAvgFrame) - 3));
            }
        }

        if(frameDataQueue.empty() && this->videoPacketQueue[tempIndex].empty() && this->decoderStatus.load() == AVPLAYER_DECODER_EOF){
            this->videoEnd = true;
        }

        tempIndex = this->queueUseIndex.load();
        if (!this->videoPacketQueue[tempIndex].empty() && frameDataQueue.size() < 5) {
            while (true) {
                if (this->videoPacketQueue[tempIndex].empty()) {
                    if (this->videoShouldFlush || this->playerShouldEnd || this->decoderStatus.load() == AVPLAYER_DECODER_EOF)break;
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
#ifdef AVPLAYER_DEBUG
            max = (this->audioPts - this->videoPts) / 1000000.0f > max ? (this->audioPts - this->videoPts) / 1000000.0f : max;
            cout << '\r' << "A-V: " << (this->audioPts - this->videoPts) / 1000000.0f << "   " << max;
#endif
            }else{
                nowC = std::chrono::system_clock::now();
                nowM = std::chrono::duration_cast<std::chrono::milliseconds>(nowC.time_since_epoch());
                if (nowM.count() - startM.count() >= 100) {
                    if (!this->userOperationQueue.empty()) {
                        finalKey = this->userOperationQueue.back();
                        this->userOperationQueue.clear();
                        tDecoderStatus = this->decoderStatus.load();
                        tPlayerStatus = this->playerStatus.load();
                        if(finalKey == Qt::Key_Space){
                            if(tPlayerStatus == AVPLAYER_AV_PLAYING){
                                this->playerStatus.store(AVPLAYER_AV_PAUSE);
                            }else if(tPlayerStatus == AVPLAYER_AV_PAUSE){
                                this->playerStatus.store(AVPLAYER_AV_PLAYING);
                            }
                        }else if(finalKey == Qt::Key_Left && !(tDecoderStatus & AVPLAYER_DECODER_BUSY)){
                            this->decoderStatus.store(AVPLAYER_DECODER_BACK);
                        }else if(finalKey == Qt::Key_Right && !(tDecoderStatus & AVPLAYER_DECODER_BUSY)){
                            this->decoderStatus.store(AVPLAYER_DECODER_ADVANCE);
                        }else if(finalKey == Qt::Key_R && !(tDecoderStatus & AVPLAYER_DECODER_BUSY)){
                            this->gotoPts.first = 0;
                            this->decoderStatus.store(AVPLAYER_DECODER_GOTO);
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
    this->playerStatus.store(AVPLAYER_AV_STOP);
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

#ifdef AVPLAYER_DEBUG
    qDebug()<<"opengl end";
#endif

    this->messagePrint("INFO::OPENGL::RENDER_END", AVPLAYER_COLOR_GREEN);
}

void MainWindow::openALoutputThread(){
    int SBD_size = 8;
    unsigned int SSD = 0;
    unsigned int SBD[SBD_size] = { 0 };
    float sourcePos[3] = { 0.0f,0.0f,0.0f };
    float sourceVel[3] = { 0.0f,0.0f,0.0f };
    int ret = -1;
    unsigned int unQueueBufferId = 0;
    unsigned char nowStatus = AVPLAYER_AV_UNKNOW;
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
            this->messagePrint("ERROR::OPENAL::NOT_DEVICE_USE", AVPLAYER_COLOR_RED);
            this->playerShouldEnd = true;
            return;
        }
    }
    if (!this->context) {
        this->context = alcCreateContext(this->device, nullptr);
        if (!this->context) {
            this->messagePrint("ERROR::OPENAL::CAN_NOT_CREATE_CONTEXT", AVPLAYER_COLOR_RED);
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
        if (nowStatus != AVPLAYER_AV_PLAYING) {
            this->audioIsWaiting = true;
            alSourcePause(SSD);
            do {
                if (this->audioShouldFlush) {
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
            } while ((nowStatus != AVPLAYER_AV_PLAYING && nowStatus != AVPLAYER_AV_STOP) || this->audioShouldFlush);
            if (audioShortBuffer) {
                ret = 2 % SBD_size;
                while (ret-- > 0) {
                    if (this->audioDataQueue[this->queueUseIndex.load()].empty()) {
                        if (this->playerStatus.load() != AVPLAYER_AV_PLAYING || this->playerShouldEnd || this->audioShouldFlush) {
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
                if(this->decoderStatus.load() == AVPLAYER_DECODER_EOF){
                    this->audioEnd = true;
                    break;
                }
                if (this->playerStatus.load() != AVPLAYER_AV_PLAYING || this->playerShouldEnd || this->audioShouldFlush) {
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

    this->messagePrint("INFO::OPENAL::OUTPUT_END", AVPLAYER_COLOR_GREEN);
}

