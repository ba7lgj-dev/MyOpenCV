import time

import cv2
import numpy as np
import requests
from io import BytesIO
import math

from utils.http import get_image_from_url
from utils.mathCount import find_midpoint, find_intersection, distance_between_points

#
# def crop_center_image(url, output_path, window_name='Processed Image'):
#     """
#     从图片中心裁剪出指定大小的区域。
#
#     参数:
#     image_path (str): 输入图片的路径。
#     output_path (str): 裁剪后图片的保存路径。
#     target_width (int): 目标宽度。
#     target_height (int): 目标高度。
#     """
#     # 读取图片
#     img = get_image_from_url(url)
#
#     # 获取图片的尺寸
#     height, width, _ = img.shape
#
#     # 计算要保留的中间十分之一高度
#     crop_height = height // 5
#
#     # 确定裁剪区域的起始和结束y坐标
#     # 为了确保裁剪的是正中间的部分，从高度的一半减去要裁剪高度的一半开始
#     start_y = height // 2 - crop_height // 2
#     end_y = start_y + crop_height
#
#     # 执行裁剪
#     cropped_img = img[start_y:end_y, :]  # ":" 表示保留全部宽度
#
#     # # 保存裁剪后的图片
#     # cv2.imwrite(output_path+"/2.jpg", cropped_img)
#
#     # 灰度图
#     grayscale_image = cv2.cvtColor(cropped_img, cv2.COLOR_BGR2GRAY)
#
#     # # 保存灰度后的图片
#     # cv2.imwrite(output_path+"/3.jpg", grayscale_image)
#
#     blurred = cv2.bilateralFilter(grayscale_image, d=5, sigmaColor=75, sigmaSpace=75)
#
#     # 二值化处理，确保背景为黑色，矩形为白色
#
#     # thresh = cv2.adaptiveThreshold(grayscale_image, 255, cv2.ADAPTIVE_THRESH_MEAN_C, cv2.THRESH_BINARY, 11, 2)
#     _, thresh = cv2.threshold(blurred, 80, 255, cv2.THRESH_BINARY)
#
#     kernel = np.ones((50, 50), np.uint8)
#     thresh = cv2.morphologyEx(thresh, cv2.MORPH_OPEN, kernel)
#
#     # 查找轮廓
#     contours, _ = cv2.findContours(thresh, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
#
#     try:
#         img_with_contours = cropped_img
#
#         # 遍历轮廓并绘制
#         for contour in contours:
#             # rect = cv2.minAreaRect(contour)
#             #
#             # # 获取最小外接矩形的中心点、尺寸和旋转角度
#             # center, size, angle = rect
#             #
#             # # 计算长边的长度作为两“竖线”间的理想距离，因为我们无法直接画出并测量垂线，这是近似方法
#             # parallel_side_length = max(size)  # 假设长边代表两竖线间距离的参考值
#             #
#             # print(f"{parallel_side_length}px")
#             #
#             # # 如果需要展示这个过程中的辅助图形，可以使用以下代码绘制最小外接矩形
#             # box = cv2.boxPoints(rect)
#             # box = np.intp(box)
#             # img_with_contours = cv2.drawContours(img_with_contours, [box], 0, (0, 99, 255), 2)
#             epsilon = 0.1 * cv2.arcLength(contour, True)
#             approx = cv2.approxPolyDP(contour, epsilon, True)
#
#             # # 绘制轮廓
#             if len(approx) == 4:
#                 cv2.drawContours(img_with_contours, [approx], 0, (0, 255, 0), 2)  # 绘制矩形轮廓
#                 #     不太正确的处理
#                 # 分别为左上 左下 右下 右上
#                 pointLT = (approx[0][0][0], approx[0][0][1])
#                 pointLB = (approx[1][0][0], approx[1][0][1])
#                 pointRB = (approx[2][0][0], approx[2][0][1])
#                 pointRT = (approx[3][0][0], approx[3][0][1])
#
#                 # 中间值 没啥用
#                 LLinemiddle = find_midpoint(pointLT, pointLB)
#                 RLinemiddle = find_midpoint(pointRB, pointRT)
#
#                 RLineIntersectionPoint = find_intersection(pointLT, pointLB, pointRT, pointRB)
#
#                 cv2.line(img_with_contours, LLinemiddle, RLineIntersectionPoint, (0, 0, 255), 2)
#                 distance = distance_between_points(LLinemiddle, RLineIntersectionPoint)
#                 realDistamce = distance * 1.375
#                 cv2.putText(img_with_contours, f"{realDistamce:.0f}mm", RLineIntersectionPoint,
#                             cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 1, cv2.LINE_AA)
#                 # img_with_contours = cv2.drawContours(img_with_contours, [contour], -1, (0,0,255), 2)  # 红色绘制轮廓，线宽为2
#                 print(distance)
#
#         cv2.imshow(window_name, img_with_contours)  # 显示处理后的图像
#         cv2.waitKey(1)
#     except Exception as e:
#         print(e)
#
#
#
# # 使用方法
# image_url = 'http://10.39.5.132/capture'
# output_path = 'images'
# while True:
#     # time.sleep(1)
#     try:
#         crop_center_image(image_url, output_path)
#     except Exception as e:
#         time.sleep(1)
#         print(e)
