#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QtSvg/QSvgRenderer>
#include <QDebug>
#include <QFile>

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    if (argc < 5) {
        qDebug() << "Usage: convert_svg <input.svg> <output_logo.png> <output_window.png> <output_ico>";
        return 1;
    }
    
    QString inputSvg = argv[1];
    QString outputLogoPng = argv[2];
    QString outputWindowPng = argv[3];
    QString outputIco = argv[4];
    
    QSvgRenderer renderer(inputSvg);
    if (!renderer.isValid()) {
        qDebug() << "Invalid SVG:" << inputSvg;
        return 1;
    }
    
    // 1. Save logo.png (128x128 for clean, elegant inline layout that doesn't overflow)
    QImage imgLogo(128, 128, QImage::Format_ARGB32);
    imgLogo.fill(Qt::transparent);
    QPainter painterLogo(&imgLogo);
    renderer.render(&painterLogo);
    painterLogo.end();
    if (imgLogo.save(outputLogoPng, "PNG")) {
        qDebug() << "Successfully saved Logo PNG (128x128) to" << outputLogoPng;
    } else {
        qDebug() << "Failed to save Logo PNG";
    }
    
    // 2. Save window_icon.png (48x48 for crisp title bar rendering)
    QImage imgWindow(48, 48, QImage::Format_ARGB32);
    imgWindow.fill(Qt::transparent);
    QPainter painterWindow(&imgWindow);
    renderer.render(&painterWindow);
    painterWindow.end();
    if (imgWindow.save(outputWindowPng, "PNG")) {
        qDebug() << "Successfully saved Window Icon PNG (48x48) to" << outputWindowPng;
    } else {
        qDebug() << "Failed to save Window Icon PNG";
    }
    
    // 3. Save app_icon.ico (256x256 for high quality Windows desktop icon)
    QImage imgIco(256, 256, QImage::Format_ARGB32);
    imgIco.fill(Qt::transparent);
    QPainter painterIco(&imgIco);
    renderer.render(&painterIco);
    painterIco.end();
    if (imgIco.save(outputIco, "ICO")) {
        qDebug() << "Successfully saved ICO (256x256) to" << outputIco;
    } else {
        qDebug() << "Failed to save ICO";
    }
    
    return 0;
}

