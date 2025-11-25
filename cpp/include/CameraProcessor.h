#pragma once

#include <opencv2/opencv.hpp>
#include <string>

struct MeasurementResult {
    cv::Mat displayFrame;
    int whiteLengthPx{0};
    double whiteLengthMm{0.0};
    int detectedRow{0};
    bool found{false};
    std::string message;
};

struct CameraSettings {
    double lineRatio{0.6};
    int thresholdValue{90};
    int brightnessOffset{0};
    double mmPerPixel{1.0};
};

class CameraProcessor {
public:
    CameraProcessor();
    explicit CameraProcessor(const CameraSettings &settings);

    MeasurementResult measure(const cv::Mat &frame) const;
    void updateSettings(const CameraSettings &settings);

private:
    CameraSettings m_settings;

    static cv::Mat cropMiddleHalf(const cv::Mat &frame);
    cv::Mat applyBrightness(const cv::Mat &frame) const;
    cv::Mat thresholdImage(const cv::Mat &frame) const;
    int resolveLineRow(int height) const;
    MeasurementResult locateWhiteSegment(const cv::Mat &thresholdImg, int lineRow) const;
    static void drawStatus(cv::Mat &image, const std::string &message);
};

