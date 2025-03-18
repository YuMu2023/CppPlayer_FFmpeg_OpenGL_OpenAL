#ifndef AVPLAYER_H
#define AVPLAYER_H

#include <QObject>
#include <QWidget>

class QLineEdit;
class QPushButton;
class MainWindow;
class QHBoxLayout;
class QVBoxLayout;
class QCheckBox;
class QLabel;

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

    QLineEdit* lineEdit_path;
    QPushButton* pushButton_open;
    QPushButton* pushButton_browse;
    QPushButton* pushButton_back;
    QPushButton* pushButton_advance;
    QPushButton* pushButton_pause;
    QPushButton* pushButton_restart;
    QCheckBox* checkBox_loop;
    QLabel* label_av;
    QHBoxLayout* hLayout_path;
    QHBoxLayout* hLayout_operate;
    QVBoxLayout* vLayout_main;
    MainWindow* glWidget;

};

#endif // AVPLAYER_H
