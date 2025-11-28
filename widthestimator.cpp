#include "widthestimator.h"
#include <algorithm>

int MultiLineCannyWidthEstimator::findEdgeFromLeft(const cv::Mat &edgeRow) const
{
    for (int x = 0; x < edgeRow.cols; ++x) {
        if (edgeRow.at<uchar>(0, x) > 0) {
            return x;
        }
    }
    return -1;
}

int MultiLineCannyWidthEstimator::findEdgeFromRight(const cv::Mat &edgeRow) const
{
    for (int x = edgeRow.cols - 1; x >= 0; --x) {
        if (edgeRow.at<uchar>(0, x) > 0) {
            return x;
        }
    }
    return -1;
}

WidthResult MultiLineCannyWidthEstimator::estimate(const cv::Mat &frame, const CameraConfig &cfg)
{
    WidthResult result;
    if (frame.empty()) return result;

    cv::Mat rotated = frame;
    cv::Mat roi = rotated(cv::Rect(0, rotated.rows / 4, rotated.cols, rotated.rows / 2)).clone();
    cv::Mat processed = roi;
    if (cfg.flipHorizontal) {
        cv::flip(processed, processed, 1);
    }
    if (cfg.flipVertical) {
        cv::flip(processed, processed, 0);
    }

    cv::Mat gray, blur;
    cv::cvtColor(processed, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, blur, cv::Size(5,5), 1.0);

    cv::Mat edges;
    double lowThresh = cfg.cannyLow > 0 ? cfg.cannyLow : 50;
    double highThresh = cfg.cannyHigh > 0 ? cfg.cannyHigh : 150;
    cv::Canny(blur, edges, lowThresh, highThresh);

    double r0 = cfg.lineRatio;
    int bandHeight = cfg.widthRegionHeight > 0 ? cfg.widthRegionHeight : std::max(1, processed.rows / 10);
    int centerRow = static_cast<int>(std::min(1.0, std::max(0.0, r0)) * processed.rows);
    int startRow = std::max(0, centerRow - bandHeight / 2);
    int endRow = std::min(processed.rows - 1, centerRow + bandHeight / 2);
    std::vector<double> widths;
    for (int row = startRow; row <= endRow; row += std::max(1, bandHeight / 5)) {
        cv::Mat edgeRow = edges.row(row);
        int left = findEdgeFromLeft(edgeRow);
        int right = findEdgeFromRight(edgeRow);
        if (left >= 0 && right >= 0 && right > left) {
            widths.push_back(right - left);
            if (!result.valid) {
                result.usedRow = row + rotated.rows / 4;
                result.leftX = left;
                result.rightX = right;
            }
        }
    }
    if (widths.empty()) {
        result.valid = false;
        return result;
    }
    std::sort(widths.begin(), widths.end());
    double widthPixels = widths[widths.size()/2];
    result.widthPixels = widthPixels;
    result.widthMM = widthPixels * cfg.mmPerPixel;
    result.valid = true;
    return result;
}

