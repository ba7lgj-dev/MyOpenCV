#include "CameraWorker.h"

#include <QImage>
#include <QMetaType>
#include <chrono>
#include <opencv2/opencv.hpp>

namespace {
constexpr int kDefaultWidth = 1280;
constexpr int kDefaultHeight = 720;
constexpr int kWaitMs = 30;
}

CameraWorker::CameraWorker(QObject *parent) : QObject(parent) {
    qRegisterMetaType<MeasurementResult>("MeasurementResult");
}

CameraWorker::~CameraWorker() {
    stop();
}

void CameraWorker::setCameraIndex(int index) {
    m_cameraIndex = index;
}

void CameraWorker::updateProcessorSettings(const CameraSettings &settings) {
    std::scoped_lock lock(m_mutex);
    m_settings = settings;
    m_processor.updateSettings(m_settings);
}

void CameraWorker::updateControlOptions(const CameraControlOptions &options) {
    std::scoped_lock lock(m_mutex);
    m_options = options;
}

void CameraWorker::setMmPerPixel(double value) {
    std::scoped_lock lock(m_mutex);
    m_settings.mmPerPixel = value;
    m_processor.updateSettings(m_settings);
}

void CameraWorker::start() {
    if (m_running.exchange(true)) {
        return;
    }
    m_thread = std::thread(&CameraWorker::run, this);
}

void CameraWorker::stop() {
    if (!m_running.exchange(false)) {
        return;
    }
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void CameraWorker::run() {
    cv::VideoCapture capture;
    capture.open(m_cameraIndex, cv::CAP_ANY);
    if (!capture.isOpened()) {
        emit statusChanged(m_cameraIndex, QStringLiteral("无法打开摄像头"));
        return;
    }
    capture.set(cv::CAP_PROP_FRAME_WIDTH, kDefaultWidth);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, kDefaultHeight);

    while (m_running.load()) {
        cv::Mat frame;
        if (!capture.read(frame)) {
            emit statusChanged(m_cameraIndex, QStringLiteral("读取帧失败"));
            continue;
        }

        CameraSettings settingsCopy;
        CameraControlOptions optionsCopy;
        {
            std::scoped_lock lock(m_mutex);
            settingsCopy = m_settings;
            optionsCopy = m_options;
        }

        m_processor.updateSettings(settingsCopy);
        MeasurementResult result = m_processor.measure(frame);

        emit frameReady(m_cameraIndex, toImage(result.displayFrame.empty() ? frame : result.displayFrame));
        emit measurementReady(m_cameraIndex, result);

        if (result.found && optionsCopy.alarmEnabled && result.whiteLengthMm < optionsCopy.alarmThresholdMm) {
            emit alarmTriggered(m_cameraIndex, result);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kWaitMs));
    }
    capture.release();
}

QImage CameraWorker::toImage(const cv::Mat &mat) const {
    if (mat.empty()) {
        return {};
    }
    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    return QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
}

