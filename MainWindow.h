#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include<QWidget>
#include<QGL>

#include <string>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <fstream>
extern "C" {
#include "libavutil/avutil.h"
}
#include"MediaUse.h"

struct AVFormatContext;
struct AVStream;
struct AVCodecContext;
struct AVPacket;
struct AVFrame;
struct SwsContext;
struct SwrContext;
struct GLFWwindow;
struct ALCdevice;
struct ALCcontext;
class QSurface;
class QTimer;
class QOpenGLFunctions_3_0;

#define AVPLAYER_DECODER_UNKNOW		 (0x00)
#define AVPLAYER_DECODER_EOF		 (0x01)
#define AVPLAYER_DECODER_ADVANCE	 (0x02)
#define AVPLAYER_DECODER_BACK		 (0x04)
#define AVPLAYER_DECODER_PAUSE		 (0x08)
#define AVPLAYER_DECODER_STOP		 (0x10)
#define AVPLAYER_DECODER_ING		 (0x20)
#define AVPLAYER_DECODER_GOTO		 (0x40)
#define AVPLAYER_DECODER_BUSY        (0x46)

#define AVPLAYER_AV_UNKNOW			 (0x00)
#define AVPLAYER_AV_PLAYING			 (0x01)
#define AVPLAYER_AV_PAUSE			 (0x02)
#define AVPLAYER_AV_STOP			 (0x04)
#define AVPLAYER_AV_CHANGE			 (0x08)
#define AVPLAYER_AV_A_FRAME          (0x10)

#define AVPLAYER_COLOR_RESET		"\033[0m"
#define AVPLAYER_COLOR_RED			"\033[31m"
#define AVPLAYER_COLOR_GREEN		"\033[32m"
#define AVPLAYER_COLOR_YELLOW		"\033[33m"
#define AVPLAYER_COLOR_BLUE			"\033[33m"

#define AVPLAYER_DEBUG


class MainWindow:public QGLWidget{
    Q_OBJECT
public:

    MainWindow(QWidget*parent = nullptr, const char* name = nullptr, bool fs = false);
    ~MainWindow();

protected:

    void initializeGL();
    void paintGL();
    void resizeGL(int width, int height);
    void keyPressEvent(QKeyEvent* e);
    void loadGLTexture(QOpenGLFunctions_3_0* openGL_funcs);

signals:
    void updateGLrender();
    void needResize();
    void toggleFullscreen(bool fs);
    void playerEnd();

public:
    static void resourceInit();
    static void releaseResource();

    void setPath(const std::string str);
    bool avOpen();
    void avStart();
    void join();
    void avStop();
    bool avPause();
    bool avResume();
    void setOffset(std::pair<int64_t, AVRational> x);
    bool setCurrentPts(std::pair<int64_t, AVRational> x);
    void avAdvance();
    void avBack();
    bool avRestart();
    bool isRunning();
    bool playerCouldBeOperate();
    uint8_t getPlayStatus();
    uint8_t getDecoderStatus();
    std::pair<int64_t, AVRational> getCurrentPts();
    std::pair<int64_t, AVRational> getDuration();

private:

    bool videoDecoderOneFrame(SwsContext*& swsContext, AVPacket*& packet, AVFrame*& frame, std::queue<MediaUse::AVDataInfo>& frameDataQueue);
    void avClear();
    void avInit();
    void ffmpegErrorPrint(int errEnum);
    void messagePrint(const char* str, const char* color);

    void ffmpegReadThread();
    void openGLrenderThread();
    void openALoutputThread();

    bool videoShouldFlush;
    bool audioShouldFlush;
    bool videoReady;
    bool audioReady;
    bool audioIsWaiting;
    bool playerShouldEnd;
    bool justCover;
    bool fullScreen;
    bool videoIsDecoding;
    bool videoEnd;
    bool audioEnd;
    static bool resourceInitOnce;
    std::atomic<uint8_t> queueUseIndex;
    std::atomic<uint8_t> queueFlushIndex;
    std::atomic<uint8_t> decoderStatus;
    std::atomic<uint8_t> playerStatus;
    std::atomic<int64_t> videoPts;
    std::atomic<int64_t> audioPts;
    float videoAvgFrame;
    int audioSampleRate;
    int videoStreamIndex;
    int audioStreamIndex;
    int windowWidth;
    int windowHeight;
    GLuint videoTexture;
    GLuint PBO[2];
    std::string path;
    std::pair<int, int> lastKey;
    std::pair<int64_t, AVRational> duration;
    std::pair<int64_t, AVRational> offset;
    std::pair<int64_t, AVRational> gotoPts;
    AVRational videoTimeBase;
    AVRational audioTimeBase;
    AVFormatContext* formatContext;
    AVStream* videoStream;
    AVStream* audioStream;
    AVCodecContext* videoCodecContext;
    AVCodecContext* audioCodecContext;
    SwrContext* swrContext;
    static ALCdevice* device;
    static ALCcontext* context;

    MediaUse::MediaDataQueue<AVPacket*> videoPacketQueue[2];
    MediaUse::MediaDataQueue<MediaUse::AVDataInfo> audioDataQueue[2];
    MediaUse::MediaDataQueue<Qt::Key> userOperationQueue;
    MediaUse::AVFifoLoop<int64_t> audioPlayingQueue;

    std::future<void>* ffmpegThread;
    std::future<void>* openGLthread;
    std::future<void>* openALthread;

    std::mutex decoderStatus_mutex;
    static std::mutex device_mutex;
    std::condition_variable decoderStatus_cv;

    QOpenGLContext* mainGLContext;
    QSurface* mainSurface;

#ifdef AVPLAYER_DEBUG
    std::ofstream log;
    std::mutex log_mutex;
#endif

};




#endif // MAINWINDOW_H
