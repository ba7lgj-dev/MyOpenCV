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

    int regionHeight = cfg.widthRegionHeight > 0 ? cfg.widthRegionHeight : frame.rows / 2;
    regionHeight = std::max(10, std::min(regionHeight, frame.rows));
    int centerY = static_cast<int>(std::clamp(cfg.lineRatio, 0.0, 1.0) * frame.rows);
    int top = centerY - regionHeight / 2;
    if (top < 0) top = 0;
    if (top + regionHeight > frame.rows) {
        top = frame.rows - regionHeight;
    }

    cv::Rect roiRect(0, top, frame.cols, regionHeight);
    cv::Mat roi = frame(roiRect).clone();
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
    double delta = 0.05;
    std::vector<double> ratios = { r0 - delta, r0, r0 + delta };
    std::vector<double> widths;
    for (double r : ratios) {
        double clamped = std::min(1.0, std::max(0.0, r));
        int row = static_cast<int>(clamped * edges.rows);
        cv::Mat edgeRow = edges.row(row);
        int left = findEdgeFromLeft(edgeRow);
        int right = findEdgeFromRight(edgeRow);
        if (left >= 0 && right >= 0 && right > left) {
            widths.push_back(right - left);
            if (!result.valid) {
                result.usedRow = roiRect.y + row;
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

