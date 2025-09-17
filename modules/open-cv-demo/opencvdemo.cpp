#include "opencvdemo.h"

#include <QLabel>

#include <QImage>
#include <QLabel>
#include <QPixmap>
#ifdef WITH_OPENCV
#include <opencv2/opencv.hpp>
#endif


REGISTER_DYNAMICOBJECT(OpenCvDemo);

OpenCvDemo::OpenCvDemo() : QWidget(nullptr), DynamicObjectBase()
{
    QVBoxLayout * layout = new QVBoxLayout;

    QLabel * label = new QLabel;

    label->setText(tr("待实现....."));

#ifdef WITH_OPENCV
    // Load an image using OpenCV
    cv::Mat cvImage = cv::imread(QString("C:\\Users\\yangxu\\Pictures\\下载.jpg").toLocal8Bit().toStdString());
    if (cvImage.empty()) {
        qDebug() << "Failed to load image.";
    }

    // Convert OpenCV image to QImage
    QImage qtImage(cvImage.data, cvImage.cols, cvImage.rows, cvImage.step, QImage::Format_RGB888);

    // Display the image using QLabel
    QLabel * label1 = new QLabel;
    label1->setPixmap(QPixmap::fromImage(qtImage));
    layout->addWidget(label1);
#else
    QLabel * label1 = new QLabel;
    label1->setText(tr("OpenCV support not available"));
    layout->addWidget(label1);
#endif

    layout->addWidget(label);
    layout->setAlignment(Qt::AlignTop);
    this->setLayout(layout);
}

