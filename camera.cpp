#include "camera.h"
#include "configmanager.h"
#include <QDateTime>
#include <QMetaType>
#include <QMutexLocker>

UsbCameraWorker::UsbCameraWorker(int index, QObject *parent)
    : QThread(parent), index(index)
{
    qRegisterMetaType<cv::Mat>("cv::Mat");
}

void UsbCameraWorker::requestStop()
{
    QMutexLocker locker(&mutex);
    running = false;
}

void UsbCameraWorker::enableAutoExposure(bool enable)
{
    QMutexLocker locker(&mutex);
    autoExp = enable;
}

bool UsbCameraWorker::autoExposureEnabled() const
{
    return autoExp;
}

void UsbCameraWorker::setConfig(const CameraConfig &cfg)
{
    if (!config) {
        config = new CameraConfig(cfg);
    } else {
        *config = cfg;
    }
}

bool UsbCameraWorker::openCamera()
{
    if (cap.isOpened()) {
        cap.release();
    }
    bool ok = cap.open(index);
    if (!ok) {
        emit cameraError(tr("Camera %1 open failed").arg(index));
        return false;
    }
    failCount = 0;
    if (config) {
        if (config->exposure >= 0) {
            cap.set(cv::CAP_PROP_EXPOSURE, config->exposure);
        }
        if (config->brightness >= 0) {
            cap.set(cv::CAP_PROP_BRIGHTNESS, config->brightness);
        }
    }
    return true;
}

void UsbCameraWorker::performAutoExposure(const cv::Mat &frame)
{
    if (!config) return;
    cv::Rect roi(0, frame.rows / 4, frame.cols, frame.rows / 2);
    cv::Mat cropped = frame(roi);
    cv::Mat gray;
    cv::cvtColor(cropped, gray, cv::COLOR_BGR2GRAY);
    cv::Scalar mean, stddev;
    cv::meanStdDev(gray, mean, stddev);
    double meanGray = mean[0];
    double exp = cap.get(cv::CAP_PROP_EXPOSURE);
    double step = config->autoExposureStep;
    if (meanGray < config->targetGrayMin && exp < config->autoExposureMax) {
        cap.set(cv::CAP_PROP_EXPOSURE, exp + step);
    } else if (meanGray > config->targetGrayMax && exp > config->autoExposureMin) {
        cap.set(cv::CAP_PROP_EXPOSURE, exp - step);
    }
}

QImage UsbCameraWorker::matToImage(const cv::Mat &mat)
{
    cv::Mat rgb;
    if (mat.channels() == 1) {
        cv::cvtColor(mat, rgb, cv::COLOR_GRAY2RGB);
    } else {
        cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    }
    return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
}

void UsbCameraWorker::run()
{
    running = true;
    while (true) {
        {
            QMutexLocker locker(&mutex);
            if (!running) break;
        }
        if (!cap.isOpened()) {
            if (!openCamera()) {
                emit cameraError(tr("Camera %1 busy or unavailable, retry in 3s").arg(index));
                msleep(3000);
                continue;
            }
        }
        cv::Mat frame;
        bool ok = cap.read(frame);
        if (!ok) {
            failCount++;
            if (failCount >= 10) {
                emit cameraError(tr("Camera disconnected, trying to reconnect..."));
                cap.release();
                msleep(500);
                if (openCamera()) {
                    emit cameraError(tr("Camera reconnected successfully."));
                } else {
                    emit cameraError(tr("Camera reconnect failed, please check USB."));
                }
            }
            continue;
        }
        failCount = 0;
        if (config) {
            if (config->flipHorizontal) {
                cv::flip(frame, frame, 1);
            }
            if (config->flipVertical) {
                cv::flip(frame, frame, 0);
            }
            switch (config->rotation) {
            case 90:
                cv::rotate(frame, frame, cv::ROTATE_90_CLOCKWISE);
                break;
            case 180:
                cv::rotate(frame, frame, cv::ROTATE_180);
                break;
            case 270:
                cv::rotate(frame, frame, cv::ROTATE_90_COUNTERCLOCKWISE);
                break;
            default:
                break;
            }
        }
        emit rawFrameReady(frame);
        emit frameReady(matToImage(frame));
        if (autoExposureEnabled()) {
            performAutoExposure(frame);
        }
        msleep(10);
    }
    cap.release();
}

UsbCamera::UsbCamera(QObject *parent)
    : ICamera(parent)
{
}

UsbCamera::~UsbCamera()
{
    stop();
    close();
    delete config;
}

bool UsbCamera::open(int idx)
{
    index = idx;
    setupWorker();
    return true;
}

void UsbCamera::close()
{
    if (worker) {
        worker->requestStop();
        worker->wait();
        worker->deleteLater();
        worker = nullptr;
    }
}

bool UsbCamera::isOpened() const
{
    return worker != nullptr;
}

void UsbCamera::start()
{
    if (!worker) return;
    if (!worker->isRunning()) {
        worker->start();
    }
}

void UsbCamera::stop()
{
    if (worker) {
        worker->requestStop();
        worker->wait();
    }
}

void UsbCamera::setConfig(const CameraConfig &cfg)
{
    if (!config) config = new CameraConfig(cfg); else *config = cfg;
    if (worker) worker->setConfig(cfg);
}

void UsbCamera::triggerAutoExposure()
{
    if (worker) {
        worker->enableAutoExposure(true);
    }
}

void UsbCamera::setupWorker()
{
    if (worker) {
        close();
    }
    worker = new UsbCameraWorker(index);
    if (config) {
        worker->setConfig(*config);
    }
    connect(worker, &UsbCameraWorker::frameReady, this, &UsbCamera::frameReady);
    connect(worker, &UsbCameraWorker::rawFrameReady, this, &UsbCamera::rawFrameReady);
    connect(worker, &UsbCameraWorker::cameraError, this, &UsbCamera::cameraError);
}

