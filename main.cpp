#include <QApplication>
#include<QMessageBox>
#include<QDir>
#include<QFileDialog>
#include<string>

//#include<gperftools/profiler.h>

#include"MainWindow.h"
#include"AVPlayer.h"

int main(int argc, char *argv[])
{

    QApplication a(argc, argv);

//    QString filePath(QFileDialog::getOpenFileName(nullptr,"select media","/home/a233/CppProjects/build-test-Desktop-Debug/resourceM","all files(*.*)"));
//    qWarning(filePath.toStdString().c_str());

//    MainWindow::resourceInit();
//    MainWindow mainwindow(nullptr, "WOW!!!!!", false);
//    mainwindow.setPath(filePath.toStdString());
//    //ProfilerStart("./prof_capture.prof");
//    if(mainwindow.avOpen()){
//        mainwindow.avStart();
//    }
//    mainwindow.show();
//    MainWindow::releaseResource();

    //ProfilerStop();

    //ProfilerStart("./prof_capture.prof");

    MainWindow::resourceInit();

    AVPlayer w;
    w.show();

    MainWindow::releaseResource();

    //ProfilerStop();

    return a.exec();
}
