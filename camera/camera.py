"""Camera helpers responsible for initialising the ESP32 camera and measuring.

The previous version of this module returned bare tuples and relied on print
statements for error handling.  The refactored implementation exposes clearer
exceptions and breaks the work into smaller helper functions so that each step
is easier to understand and to test.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, Tuple

import cv2
import numpy as np

import utils.http as http
from utils.http import get_image_from_url


class CameraProcessingError(RuntimeError):
    """Raised when the camera feed cannot be processed."""


@dataclass
class Measurement:
    frame: np.ndarray
    white_length: int


def init(base_url: str) -> Dict[str, str]:
    """Initialise the camera with a set of predefined parameters."""

    if not base_url:
        return {}

    init_define_list = {
        "设置分辨率": "/control?var=framesize&val=13",
    }

    results: Dict[str, str] = {}
    for key, item in init_define_list.items():
        full_url = f"{base_url}{item}"
        response = http.send_get_request(full_url)
        results[key] = str(response.status_code) if response else "请求失败"
    return results


def getImage(base_url: str) -> Tuple[np.ndarray, int]:
    """Fetch a frame from the camera and measure the dominant white segment."""

    measurement = _measure_white_segment(base_url)
    return measurement.frame, measurement.white_length


def _measure_white_segment(base_url: str) -> Measurement:
    if not base_url:
        raise CameraProcessingError("缺少摄像头地址")

    image = get_image_from_url(f"{base_url}/capture")
    if image is None:
        raise CameraProcessingError("无法获取图像数据")

    cropped = _crop_middle_half(image)
    threshold = _threshold_image(cropped)
    start, end = _locate_main_white_segment(threshold)

    result_img = cv2.cvtColor(threshold, cv2.COLOR_GRAY2BGR)
    height, _ = threshold.shape
    cv2.line(result_img, (0, height // 2), (result_img.shape[1] - 1, height // 2), (0, 255, 0), 1)
    start_point = (start, height // 2)
    end_point = (end, height // 2)
    cv2.line(result_img, start_point, end_point, (0, 0, 255), 2)

    white_length = end - start + 1
    text = f"{white_length}px"
    text_size = cv2.getTextSize(text, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 1)[0]
    text_x = (start_point[0] + end_point[0]) // 2 - text_size[0] // 2
    text_y = height // 2 - 10
    cv2.putText(result_img, text, (text_x, text_y), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 1)

    return Measurement(frame=result_img, white_length=white_length)


def _crop_middle_half(image: np.ndarray) -> np.ndarray:
    """Keep the middle half of the image vertically where the tape is located."""

    height = image.shape[0]
    segment_height = height // 4
    start = segment_height
    end = 3 * segment_height
    return image[start:end, :]


def _threshold_image(image: np.ndarray) -> np.ndarray:
    gray_img = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
    _, threshold_img = cv2.threshold(gray_img, 90, 255, cv2.THRESH_BINARY)
    return threshold_img


def _locate_main_white_segment(threshold_img: np.ndarray) -> Tuple[int, int]:
    height, width = threshold_img.shape
    center_row = threshold_img[height // 2, :]
    white_pixels = np.where(center_row == 255)[0]
    if white_pixels.size == 0:
        raise CameraProcessingError("中间行没有检测到白色像素")

    margin = int(width * 0.05)
    filtered = white_pixels[(white_pixels >= margin) & (white_pixels <= width - margin)]
    if filtered.size == 0:
        raise CameraProcessingError("过滤噪点后没有剩余白色像素")

    changes = np.diff(filtered.astype(int)) > 1
    segments = np.split(filtered, np.where(changes)[0] + 1)
    if not segments:
        raise CameraProcessingError("未检测到连续的白色区域")

    main_segment = max(segments, key=lambda x: len(x))
    return int(main_segment[0]), int(main_segment[-1])
