# http_requests.py
import time
from io import BytesIO

import cv2
import numpy as np
import requests
import urllib3

from utils.logging_utils import get_logger
from utils.notifications import OPERATIONS_WEBHOOK, notifier


logger = get_logger(__name__)


def send_wechat_work_message(webhook_url, message_content):
    """Send a text notification through the shared notifier."""

    notifier.send_text(webhook_url, message_content)

def send_get_request(url, params=None, headers=None):
    """
    发送GET请求到指定的URL。

    :param url: str, 请求的URL地址。
    :param params: dict, 查询参数，可选。
    :param headers: dict, 请求头信息，可选。
    :return: requests.Response 对象，包含服务器响应的内容。
    """
    try:
        response = requests.get(url, params=params, headers=headers)
        response.raise_for_status()  # 如果响应状态码不是200，将抛出HTTPError异常
        return response
    except requests.RequestException as e:
        logger.exception("请求发生错误: %s", e)
        return None

RETRY_DELAY_MS = 1000


def get_image_from_url(url, max_retries=10, timeout=5):
    retries = 0
    while True:
        try:
            # 发送GET请求获取图像数据，设置超时时间
            response = requests.get(url, stream=True, timeout=timeout)
            if response.status_code == 200:
                # 将响应内容转换为BytesIO对象，以便OpenCV处理
                image_stream = BytesIO(response.content)
                # 将BytesIO对象转换为numpy数组，供OpenCV使用
                nparr = np.frombuffer(image_stream.getvalue(), np.uint8)
                # 解码numpy数组为图像
                image = cv2.imdecode(nparr, cv2.IMREAD_COLOR)
                if image is not None:
                    response.close()
                    notifier.notify_recovery("camera_stream", OPERATIONS_WEBHOOK, "摄像头连接已恢复")
                    return image
                else:
                    logger.error("无法解码图像数据")
            else:
                logger.warning("请求失败，状态码：%s", response.status_code)
            response.close()
        except (requests.exceptions.RequestException, urllib3.exceptions.ProtocolError) as e:
            logger.warning("请求出错: %s，正在重试 %d...", e, retries + 1)
            short_error = str(e)
            notifier.notify_error(
                "camera_stream",
                OPERATIONS_WEBHOOK,
                f"摄像头异常，重试中：{short_error}",
                escalate_after=3,
            )
            retries += 1
            logger.info("摄像头请求失败，%d ms 后重试", RETRY_DELAY_MS)
            time.sleep(RETRY_DELAY_MS / 1000.0)
    logger.error("达到最大重试次数，请求失败。")
