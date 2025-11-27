#ifndef WIDTHESTIMATOR_H
#define WIDTHESTIMATOR_H

#include <opencv2/opencv.hpp>
#include "configmanager.h"

struct WidthResult {
    double widthPixels {0.0};
    double widthMM {0.0};
    bool valid {false};
    int usedRow {0};
    int leftX {0};
    int rightX {0};
};

class IWidthEstimator {
public:
    virtual ~IWidthEstimator() = default;
    virtual WidthResult estimate(const cv::Mat &frame, const CameraConfig &cfg) = 0;
};

class MultiLineCannyWidthEstimator : public IWidthEstimator {
public:
    WidthResult estimate(const cv::Mat &frame, const CameraConfig &cfg) override;

private:
    int findEdgeFromLeft(const cv::Mat &edgeRow) const;
    int findEdgeFromRight(const cv::Mat &edgeRow) const;
};

#endif // WIDTHESTIMATOR_H
