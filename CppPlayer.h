#ifndef CPPLAYER_H
#define CPPLAYER_H



/**
* @Author:       Li
* @Version:      1.0
* @Date:         2025-03-26
* @Description:  CppPlayer类的声明
**/


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


/**
* @Author:       Li
* @Version:      1.0
* @Date:         2025-03-26
* @Description:  解码器状态和播放状态的define
**/
#define CPPPLAYER_DEFINE
#define CPPPLAYER_DECODER_UNKNOW     (0x00)
#define CPPPLAYER_DECODER_EOF		 (0x01)
#define CPPPLAYER_DECODER_ADVANCE	 (0x02)
#define CPPPLAYER_DECODER_BACK		 (0x04)
#define CPPPLAYER_DECODER_PAUSE		 (0x08)
#define CPPPLAYER_DECODER_STOP		 (0x10)
#define CPPPLAYER_DECODER_ING		 (0x20)
#define CPPPLAYER_DECODER_GOTO		 (0x40)
#define CPPPLAYER_DECODER_BUSY       (0x46)

#define CPPPLAYER_AV_UNKNOW			 (0x00)
#define CPPPLAYER_AV_PLAYING	     (0x01)
#define CPPPLAYER_AV_PAUSE			 (0x02)
#define CPPPLAYER_AV_STOP			 (0x04)
#define CPPPLAYER_AV_CHANGE			 (0x08)
#define CPPPLAYER_AV_A_FRAME         (0x10)

//std::cout输出字符颜色修改
#define CPPPLAYER_COLOR_RESET		"\033[0m"
#define CPPPLAYER_COLOR_RED			"\033[31m"
#define CPPPLAYER_COLOR_GREEN		"\033[32m"
#define CPPPLAYER_COLOR_YELLOW		"\033[33m"
#define CPPPLAYER_COLOR_BLUE		"\033[33m"

//define开启debug，不需要请注释
#define CPPPLAYER_DEBUG



/**
* @Author:       Li
* @Version:      1.0
* @Date:         2025-03-26
* @Description:  能够独立运行或嵌入其他Qt窗口的player类，使用了OpenGL、OpenAL、FFmpeg库
**/
class CppPlayer:public QGLWidget{
    Q_OBJECT
public:

    CppPlayer(QWidget*parent = nullptr, const char* name = nullptr, bool fs = false);
    ~CppPlayer();

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

    //跳转时给渲染或音频输出线程刷新信号，即告诉线程队列的数据是过时或超时的，需要清空和切换队列
    bool videoShouldFlush;
    bool audioShouldFlush;

    //表示渲染或音频输出准备完毕，随时可以开始
    bool videoReady;
    bool audioReady;

    //表示音频线程正在等待某些条件而暂停输出
    bool audioIsWaiting;

    //表示是否该结束播放和解码了，用于控制整个播放器的随时退出
    bool playerShouldEnd;

    //表示视频流是否只有一张图片（或mp3封面）
    bool justCover;

    //是否需要全屏
    bool fullScreen;

    //视频渲染线程当前是否在使用解码器，避免在使用解码器时解码线程对解码器进行刷新操作导致视频解码异常
    bool videoIsDecoding;

    //判断音视频是否播放完毕，以便在设置循环播放模式下重新解码播放
    bool videoEnd;
    bool audioEnd;

    //资源初始化，避免多次对音频输出设备初始化
    static bool resourceInitOnce;

    //对在多线程中经常读写的变量操作原子化
    std::atomic<uint8_t> queueUseIndex;
    std::atomic<uint8_t> queueFlushIndex;
    std::atomic<uint8_t> decoderStatus;
    std::atomic<uint8_t> playerStatus;
    std::atomic<int64_t> videoPts;
    std::atomic<int64_t> audioPts;

    //文件解码信息
    float videoAvgFrame;
    int audioSampleRate;
    int videoStreamIndex;
    int audioStreamIndex;
    int windowWidth;
    int windowHeight;

    //OpenGL资源
    GLuint videoTexture;
    GLuint PBO[2];

    //文件路径或地址
    std::string path;

    //用于纯OpenGL键盘读取（已经弃用）
    std::pair<int, int> lastKey;

    //音视频时长 <size,单位>
    std::pair<int64_t, AVRational> duration;

    //单次前进或后退偏移时长
    std::pair<int64_t, AVRational> offset;

    //指定跳转的pts
    std::pair<int64_t, AVRational> gotoPts;

    //OpenGL、OpenAL资源
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

    //视频流包队列、音频流帧队列，采用双队列机制，确保跳转时ffmpeg无需等待两个子线程放弃或清空当前队列，直接读取和解码到另一个队列
    MediaUse::MediaDataQueue<AVPacket*> videoPacketQueue[2];
    MediaUse::MediaDataQueue<MediaUse::AVDataInfo> audioDataQueue[2];

    //用户按键操作队列
    MediaUse::MediaDataQueue<Qt::Key> userOperationQueue;

    //用于当前音频播放帧的pts存储，即OpenAL音频输出缓存队列有空时拿出一个buffer并填充新数据后入队SourceQueue，这时候audioPlayingQueue同步也pop一个push一个
    MediaUse::AVFifoLoop<int64_t> audioPlayingQueue;

    //三个线程，每次更换文件播放会重新new
    std::future<void>* ffmpegThread;
    std::future<void>* openGLthread;
    std::future<void>* openALthread;

    //解码状态锁，ffmpeg线程解码完后会使用条件变量等待其他信号激活
    std::mutex decoderStatus_mutex;
    std::condition_variable decoderStatus_cv;

    //OpenAL静态设备锁
    static std::mutex device_mutex;

    //当前OpenGL上下文
    QOpenGLContext* mainGLContext;
    QSurface* mainSurface;

    //debug日志
#ifdef CPPPLAYER_DEBUG
    std::ofstream log;
    std::mutex log_mutex;
#endif

};




#endif // CPPLAYER_H
