#pragma once

#include "CameraProcessor.h"

#include <QObject>
#include <QImage>
#include <atomic>
#include <mutex>
#include <thread>

struct CameraControlOptions {
    double alarmThresholdMm{0.0};
    bool alarmEnabled{false};
    bool autoInflate{false};
    double inflateMs{600.0};
    QString cp2102Port;
};

class CameraWorker : public QObject {
    Q_OBJECT
public:
    explicit CameraWorker(QObject *parent = nullptr);
    ~CameraWorker();

    void setCameraIndex(int index);
    void updateProcessorSettings(const CameraSettings &settings);
    void updateControlOptions(const CameraControlOptions &options);
    void setMmPerPixel(double value);

public slots:
    void start();
    void stop();

signals:
    void frameReady(int index, const QImage &image);
    void measurementReady(int index, const MeasurementResult &result);
    void alarmTriggered(int index, const MeasurementResult &result);
    void statusChanged(int index, const QString &message);

private:
    void run();
    QImage toImage(const cv::Mat &mat) const;

    int m_cameraIndex{0};
    std::atomic<bool> m_running{false};
    std::thread m_thread;
    mutable std::mutex m_mutex;
    CameraProcessor m_processor;
    CameraSettings m_settings;
    CameraControlOptions m_options;
};

