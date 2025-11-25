#include "CameraProcessor.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace {
constexpr int kMarginPercent = 5;
}

CameraProcessor::CameraProcessor() : m_settings{} {}

CameraProcessor::CameraProcessor(const CameraSettings &settings) : m_settings(settings) {
    m_settings.lineRatio = std::clamp(m_settings.lineRatio, 0.0, 1.0);
    m_settings.thresholdValue = std::clamp(m_settings.thresholdValue, 0, 255);
}

void CameraProcessor::updateSettings(const CameraSettings &settings) {
    m_settings = settings;
    m_settings.lineRatio = std::clamp(m_settings.lineRatio, 0.0, 1.0);
    m_settings.thresholdValue = std::clamp(m_settings.thresholdValue, 0, 255);
}

MeasurementResult CameraProcessor::measure(const cv::Mat &frame) const {
    MeasurementResult result;

    if (frame.empty()) {
        result.message = "摄像头帧为空";
        return result;
    }

    cv::Mat adjusted = applyBrightness(frame);
    cv::Mat cropped = cropMiddleHalf(adjusted);
    cv::Mat thresholdImg = thresholdImage(cropped);
    int lineRow = resolveLineRow(thresholdImg.rows);

    result = locateWhiteSegment(thresholdImg, lineRow);
    if (result.found) {
        result.whiteLengthMm = result.whiteLengthPx * m_settings.mmPerPixel;
    }
    return result;
}

cv::Mat CameraProcessor::cropMiddleHalf(const cv::Mat &frame) {
    if (frame.empty()) {
        return frame;
    }
    int segmentHeight = frame.rows / 4;
    int start = segmentHeight;
    int end = 3 * segmentHeight;
    cv::Rect roi(0, start, frame.cols, end - start);
    return frame(roi).clone();
}

cv::Mat CameraProcessor::applyBrightness(const cv::Mat &frame) const {
    if (frame.empty() || m_settings.brightnessOffset == 0) {
        return frame;
    }
    cv::Mat adjusted;
    frame.convertTo(adjusted, -1, 1, m_settings.brightnessOffset);
    return adjusted;
}

cv::Mat CameraProcessor::thresholdImage(const cv::Mat &frame) const {
    cv::Mat gray;
    cv::cvtColor(frame, gray, cv::COLOR_BGR2GRAY);
    cv::Mat thresholdImg;
    cv::threshold(gray, thresholdImg, m_settings.thresholdValue, 255, cv::THRESH_BINARY);
    return thresholdImg;
}

int CameraProcessor::resolveLineRow(int height) const {
    if (height <= 1) {
        return 0;
    }
    return static_cast<int>(std::round(m_settings.lineRatio * (height - 1)));
}

MeasurementResult CameraProcessor::locateWhiteSegment(const cv::Mat &thresholdImg, int lineRow) const {
    MeasurementResult result;
    if (thresholdImg.empty()) {
        result.message = "阈值图像为空";
        return result;
    }

    const int height = thresholdImg.rows;
    const int width = thresholdImg.cols;
    int targetRow = std::clamp(lineRow, 0, height - 1);

    std::vector<int> offsets{0};
    for (int step = 1; step < height; ++step) {
        if (targetRow + step < height) offsets.push_back(step);
        if (targetRow - step >= 0) offsets.push_back(-step);
    }

    const int margin = static_cast<int>(width * (kMarginPercent / 100.0));
    for (int offset : offsets) {
        int currentRow = targetRow + offset;
        const uchar *rowPtr = thresholdImg.ptr<uchar>(currentRow);
        std::vector<int> whitePixels;
        whitePixels.reserve(width);
        for (int col = margin; col < width - margin; ++col) {
            if (rowPtr[col] == 255) {
                whitePixels.push_back(col);
            }
        }

        if (whitePixels.empty()) {
            continue;
        }

        std::vector<int> segments;
        segments.push_back(0);
        for (size_t i = 1; i < whitePixels.size(); ++i) {
            if (whitePixels[i] - whitePixels[i - 1] > 1) {
                segments.push_back(static_cast<int>(i));
            }
        }
        segments.push_back(static_cast<int>(whitePixels.size()));

        size_t bestStart = 0;
        size_t bestEnd = 0;
        size_t bestLength = 0;
        for (size_t idx = 0; idx + 1 < segments.size(); ++idx) {
            size_t startIdx = segments[idx];
            size_t endIdx = segments[idx + 1];
            size_t length = endIdx - startIdx;
            if (length > bestLength) {
                bestLength = length;
                bestStart = startIdx;
                bestEnd = endIdx - 1;
            }
        }

        if (bestLength == 0) {
            continue;
        }

        int startCol = whitePixels[bestStart];
        int endCol = whitePixels[bestEnd];

        cv::Mat display;
        cv::cvtColor(thresholdImg, display, cv::COLOR_GRAY2BGR);
        cv::line(display, cv::Point(0, currentRow), cv::Point(width - 1, currentRow), cv::Scalar(0, 255, 0), 1);
        cv::line(display, cv::Point(startCol, currentRow), cv::Point(endCol, currentRow), cv::Scalar(0, 0, 255), 2);

        int lengthPx = endCol - startCol + 1;
        std::string label = std::to_string(lengthPx) + "px";
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        int textX = (startCol + endCol) / 2 - textSize.width / 2;
        int textY = std::max(20, currentRow - 10);
        cv::putText(display, label, cv::Point(textX, textY), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(255, 0, 0), 1);

        result.displayFrame = display;
        result.whiteLengthPx = lengthPx;
        result.detectedRow = currentRow;
        result.found = true;
        return result;
    }

    cv::cvtColor(thresholdImg, result.displayFrame, cv::COLOR_GRAY2BGR);
    cv::line(result.displayFrame, cv::Point(0, targetRow), cv::Point(width - 1, targetRow), cv::Scalar(0, 255, 0), 1);
    drawStatus(result.displayFrame, "未找到合适的白色区域");
    result.message = "未找到合适的白色区域";
    result.detectedRow = targetRow;
    return result;
}

void CameraProcessor::drawStatus(cv::Mat &image, const std::string &message) {
    if (image.empty()) return;
    int height = image.rows;
    int width = image.cols;
    cv::Mat overlay = image.clone();
    cv::rectangle(overlay, cv::Point(0, height - 40), cv::Point(width, height), cv::Scalar(0, 0, 0), cv::FILLED);
    cv::addWeighted(overlay, 0.6, image, 0.4, 0, image);
    cv::putText(image, message, cv::Point(10, height - 15), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 255), 1, cv::LINE_AA);
}

