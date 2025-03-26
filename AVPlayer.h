#ifndef AVPLAYER_H
#define AVPLAYER_H


/**
* @Author:       Li
* @Version:      1.0
* @Date:         2025-03-26
* @Description:  AVPlayer类的声明
**/

#include <QObject>
#include <QWidget>

class QLineEdit;
class QPushButton;
class CppPlayer;
class QHBoxLayout;
class QVBoxLayout;
class QCheckBox;
class QLabel;



/**
* @Author:       Li
* @Version:      1.0
* @Date:         2025-03-26
* @Description:  主窗口，调用CppPlayer，为其提供文件路径，执行操作，接收并处理信号
*                用户交互层
**/
class AVPlayer : public QWidget{
public:

    AVPlayer(QWidget* parent = nullptr);
    ~AVPlayer();

private:

    void make_connections();

private slots:

    void pushButton_open_clicked();
    void pushButton_browse_clicked();
    void pushButton_back_clicked();
    void pushButton_advance_clicked();
    void pushButton_pause_clicked();
    void pushButton_restart_clicked();
    void label_av_update();

    void toggleFullscreen(bool fs);
    void updateGL();
    void shouldLoop();

private:

    QLineEdit* lineEdit_path;//编辑框，用于输入文件路径
    QPushButton* pushButton_open;//播放按键
    QPushButton* pushButton_browse;//浏览按键，用于选取文件路径
    QPushButton* pushButton_back;//后退按键
    QPushButton* pushButton_advance;//快进按键
    QPushButton* pushButton_pause;//暂停按键
    QPushButton* pushButton_restart;//重播按键
    QCheckBox* checkBox_loop;//循环选择框
    QLabel* label_av;//实时显示播放时间

    //布局
    /**
    *          __________________________________________________________
    *         |                                                          |
    *         |                                                          |  /
    *         |                                                          | /
    *         |                                                          |/_____________________ vLayout_main
    *         |                      glWidget                            |\
    *         |                                                          | \
    *         |                                                          |  \
    *         |                                                          |
    *         |__________________________________________________________|
    *         |                                                          |
    *         |                     hLayout_path                         |
    *         |__________________________________________________________|
    *         |                                                          |
    *         |                     hLayout_operate                      |
    *         |__________________________________________________________|
    *
    **/
    QHBoxLayout* hLayout_path;
    QHBoxLayout* hLayout_operate;
    QVBoxLayout* vLayout_main;

    CppPlayer* glWidget;

};

#endif // AVPLAYER_H
