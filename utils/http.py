# http_requests.py
import json
import time
from io import BytesIO

import cv2
import numpy as np
import requests
import urllib3


def send_wechat_work_message(webhook_url, message_content):
    """
    向企业微信 webhook 发送文本消息
    :param webhook_url: 企业微信机器人的 webhook 地址
    :param message_content: 消息内容
    """
    headers = {'Content-Type': 'application/json'}
    data = {
        "msgtype": "text",
        "text": {
            "content": message_content
        }
    }
    response = requests.post(webhook_url, headers=headers, data=json.dumps(data))

    # 检查响应状态码
    if response.status_code != 200:
        print(f"发送失败，错误码：{response.status_code}, 错误信息：{response.text}")
    else:
        print("消息发送成功")

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
        print(f"请求发生错误: {e}")
        return None

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
                    return image
                else:
                    print("无法解码图像数据")
            else:
                print(f"请求失败，状态码：{response.status_code}")
            response.close()
        except (requests.exceptions.RequestException, urllib3.exceptions.ProtocolError) as e:
            print(f"请求出错: {e}，正在重试 {retries + 1}...")
            webhook_url = "https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=1ffec59d-3ef7-4fc7-939f-2c69dd0d7aa6"
            message = f"请求出错: {e}，正在重试 {retries + 1}..."
            send_wechat_work_message(webhook_url, message)
            retries += 1
            time.sleep(1)  # 等待1秒后重试
    print("达到最大重试次数，请求失败。")
