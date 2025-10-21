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


_DEFAULT_LINE_POSITION_RATIO = 0.6


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


def getImage(base_url: str, line_position_ratio: float | None = None) -> Tuple[np.ndarray, int]:
    """Fetch a frame from the camera and measure the dominant white segment."""

    measurement = _measure_white_segment(base_url, line_position_ratio)
    return measurement.frame, measurement.white_length


def _measure_white_segment(base_url: str, line_position_ratio: float | None) -> Measurement:
    if not base_url:
        raise CameraProcessingError("缺少摄像头地址")

    image = get_image_from_url(f"{base_url}/capture")
    if image is None:
        raise CameraProcessingError("无法获取图像数据")

    cropped = _crop_middle_half(image)
    threshold = _threshold_image(cropped)
    height, width = threshold.shape
    line_row = _resolve_line_row(height, line_position_ratio)
    try:
        start, end, detected_row = _locate_main_white_segment(threshold, line_row)
    except CameraProcessingError as exc:
        result_img = cv2.cvtColor(threshold, cv2.COLOR_GRAY2BGR)
        cv2.line(result_img, (0, line_row), (width - 1, line_row), (0, 255, 0), 1)
        _draw_status_text(result_img, str(exc))
        return Measurement(frame=result_img, white_length=0)

    result_img = cv2.cvtColor(threshold, cv2.COLOR_GRAY2BGR)
    line_row = detected_row
    cv2.line(result_img, (0, line_row), (width - 1, line_row), (0, 255, 0), 1)
    start_point = (start, line_row)
    end_point = (end, line_row)
    cv2.line(result_img, start_point, end_point, (0, 0, 255), 2)

    white_length = end - start + 1
    text = f"{white_length}px"
    text_size = cv2.getTextSize(text, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 1)[0]
    text_x = (start_point[0] + end_point[0]) // 2 - text_size[0] // 2
    text_y = max(20, line_row - 10)
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


def _resolve_line_row(height: int, line_position_ratio: float | None) -> int:
    if line_position_ratio is None:
        ratio = _DEFAULT_LINE_POSITION_RATIO
    else:
        try:
            ratio = float(line_position_ratio)
        except (TypeError, ValueError):  # noqa: PERF203 - guard against invalid inputs
            ratio = _DEFAULT_LINE_POSITION_RATIO
    ratio = max(0.0, min(1.0, ratio))
    if height <= 1:
        return 0
    return int(round(ratio * (height - 1)))


def _locate_main_white_segment(threshold_img: np.ndarray, line_row: int) -> Tuple[int, int, int]:
    height, width = threshold_img.shape
    if height == 0:
        raise CameraProcessingError("图像高度异常")

    target_row = max(0, min(height - 1, int(line_row)))
    search_offsets = [0]
    for step in range(1, height):
        if target_row + step < height:
            search_offsets.append(step)
        if target_row - step >= 0:
            search_offsets.append(-step)

    margin = int(width * 0.05)
    for offset in search_offsets:
        current_row = target_row + offset
        row_pixels = threshold_img[current_row, :]
        white_pixels = np.where(row_pixels == 255)[0]
        if white_pixels.size == 0:
            continue

        filtered = white_pixels[(white_pixels >= margin) & (white_pixels <= width - margin)]
        if filtered.size == 0:
            continue

        changes = np.diff(filtered.astype(int)) > 1
        segments = np.split(filtered, np.where(changes)[0] + 1)
        if not segments:
            continue

        main_segment = max(segments, key=lambda x: len(x))
        return int(main_segment[0]), int(main_segment[-1]), current_row

    raise CameraProcessingError("附近行未检测到合适的白色区域")


def _draw_status_text(image: np.ndarray, message: str) -> None:
    height, width, _ = image.shape
    overlay = image.copy()
    cv2.rectangle(overlay, (0, height - 40), (width, height), (0, 0, 0), thickness=-1)
    cv2.addWeighted(overlay, 0.6, image, 0.4, 0, dst=image)
    cv2.putText(
        image,
        message,
        (10, height - 15),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.5,
        (0, 0, 255),
        1,
        cv2.LINE_AA,
    )
