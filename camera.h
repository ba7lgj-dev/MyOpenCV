#ifndef CAMERA_H
#define CAMERA_H

#include <QObject>
#include <QImage>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QList>
#include <opencv2/opencv.hpp>

class CameraConfig;

class ICamera : public QObject {
    Q_OBJECT
public:
    explicit ICamera(QObject *parent = nullptr) : QObject(parent) {}
    ~ICamera() override = default;
    virtual bool open(int index) = 0;
    virtual void close() = 0;
    virtual bool isOpened() const = 0;

public slots:
    virtual void start() = 0;
    virtual void stop() = 0;

signals:
    void frameReady(const QImage &frame);
    void rawFrameReady(const cv::Mat &frame);
    void cameraError(const QString &msg);
};

class UsbCameraWorker : public QThread {
    Q_OBJECT
public:
    UsbCameraWorker(int index, QObject *parent = nullptr);
    void requestStop();
    void enableAutoExposure(bool enable);
    bool autoExposureEnabled() const;
    void setConfig(const CameraConfig &cfg);

signals:
    void frameReady(const QImage &frame);
    void rawFrameReady(const cv::Mat &frame);
    void cameraError(const QString &msg);

protected:
    void run() override;

private:
    bool openCamera();
    void performAutoExposure(const cv::Mat &frame);
    QImage matToImage(const cv::Mat &mat);

    int index;
    cv::VideoCapture cap;
    bool running {false};
    bool autoExp {false};
    int failCount {0};
    CameraConfig *config {nullptr};
    QMutex mutex;
};

class UsbCamera : public ICamera {
    Q_OBJECT
public:
    explicit UsbCamera(QObject *parent = nullptr);
    ~UsbCamera() override;

    bool open(int index) override;
    void close() override;
    bool isOpened() const override;
    static QList<int> scanAvailable(int maxIndex = 5);

public slots:
    void start() override;
    void stop() override;
    void setConfig(const CameraConfig &cfg);
    void triggerAutoExposure();

private:
    void setupWorker();

    int index {-1};
    UsbCameraWorker *worker {nullptr};
    CameraConfig *config {nullptr};
};

#endif // CAMERA_H
