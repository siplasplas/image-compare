#include "MainWindow.h"

#include <QApplication>
#include <QStyleFactory>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QApplication::setStyle(QStyleFactory::create("Fusion")); // non-native Qt style
    QApplication::setApplicationName("image-compare");

    MainWindow w;
    w.show();

    const QStringList args = QApplication::arguments();
    if (args.size() >= 3) {
        w.setImages(args.at(1), args.at(2));
    }

    return QApplication::exec();
}