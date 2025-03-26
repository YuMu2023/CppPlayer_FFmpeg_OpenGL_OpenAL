#include<QApplication>
#include<QMessageBox>
#include<QDir>
#include<QFileDialog>
#include<string>

#include"CppPlayer.h"
#include"AVPlayer.h"

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);

    //资源初始化
    CppPlayer::resourceInit();

    AVPlayer w;
    w.show();

    //资源释放
    CppPlayer::releaseResource();


    return a.exec();
}
