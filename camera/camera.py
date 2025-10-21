import utils.http as http
import cv2
import numpy as np
from utils.http import get_image_from_url
from utils.mathCount import find_midpoint, find_intersection, distance_between_points


"""
初始化摄像头
"""
def init(url):
    # 初始化定义列表
    init_define_list = {
        "设置分辨率": "/control?var=framesize&val=13",
        # "亮度": "/control?var=brightness&val=-2",
        # "对比度": "/control?var=contrast&val=1",
        # "自动曝光级别": "/control?var=ae_level&val=2",
    }


    # # 循环发送请求并将结果（状态码）写回字典
    for key, item in init_define_list.items():
        full_url = url + item
        response = http.send_get_request(full_url)
        # 存储每个请求的状态码到字典中
        init_define_list[key] = response.status_code if response else "请求失败"
    return init_define_list


# class ImageDownloadError:
#     pass


def getImage(url):
    img = get_image_from_url(url + "/capture")

    # 获取图片的尺寸
    height, width, _ = img.shape

    # 计算每段的高度
    segment_height = height // 4

    # 裁切中间的两段
    # 中间两段的起始和结束行
    start_row_middle1 = segment_height
    end_row_middle1 = 3 * segment_height

    # 裁切中间的第一段
    img = img[start_row_middle1:end_row_middle1, :]

    gray_img = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    # 二值化
    # 参数：1.原图像， 2.进行分类的阈值，3.高于（低于）阈值时赋予的新值，4.方法选择参数
    # 返回值：1.得到的阈值，2.阈值化后的图像
    retval, threshold_img = cv2.threshold(gray_img, 90, 255, cv2.THRESH_BINARY)
    cv2.imwrite("result" + "/threshold_img" + ".jpg", threshold_img)
    # 获取图像高度和宽度
    height, width = threshold_img.shape
    # 取正中间一行
    center_row = threshold_img[height // 2, :]

    # 找到所有白色像素的位置（值为255）
    white_pixels = np.where(center_row == 255)[0]

    if len(white_pixels) == 0:
        print("中间行没有检测到白色像素")

    # 处理边缘噪点 - 忽略两端各5%的像素作为噪点容差
    margin = int(width * 0.05)
    filtered_white = white_pixels[(white_pixels >= margin) & (white_pixels <= width - margin)]

    if len(filtered_white) == 0:
        print("过滤噪点后没有剩余白色像素")

    # 计算连续白色区域的起始和结束位置
    changes = np.diff(filtered_white.astype(int)) > 1
    segments = np.split(filtered_white, np.where(changes)[0] + 1)

    # 找出最长的连续白色区域（主白色部分）
    main_segment = max(segments, key=lambda x: len(x))

    # 计算主白色区域的长度
    white_length = main_segment[-1] - main_segment[0] + 1

    print(f"中间行主白色部分长度: {white_length} 像素")

    # 在原图上绘制测量结果
    result_img = cv2.cvtColor(threshold_img, cv2.COLOR_GRAY2BGR)

    # 绘制中间线
    cv2.line(result_img, (0, height // 2), (width - 1, height // 2), (0, 255, 0), 1)

    # 绘制白色区域边界
    start_point = (int(main_segment[0]), height // 2)
    end_point = (int(main_segment[-1]), height // 2)
    cv2.line(result_img, start_point, end_point, (0, 0, 255), 2)

    # 添加长度标注
    text = f"{white_length}px"
    text_size = cv2.getTextSize(text, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 1)[0]
    text_x = (start_point[0] + end_point[0]) // 2 - text_size[0] // 2
    text_y = height // 2 - 10
    cv2.putText(result_img, text, (text_x, text_y),
                cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 1)

    return result_img,white_length



#
# def getLength(url,rate):
#
#     # 读取图片
#     img = get_image_from_url(url+"/capture")
#
#     # 获取图片的尺寸
#     height, width, _ = img.shape
#
#     # 计算要保留的宽度（右侧三分之二）
#     crop_width = int(width * 2 / 3)
#
#     # 计算要保留的高度（中间6/8=3/4）
#     crop_height = int(height * 6 / 8)
#
#     # 确定裁剪区域的坐标
#     start_x = width - crop_width
#     start_y = int((height - crop_height) / 2)
#     end_y = start_y + crop_height
#
#     # 执行裁剪
#     cropped_img = img[start_y:end_y, start_x:width]
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
#     kernel = np.ones((20, 20), np.uint8)
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
#                 realDistamce = distance * rate
#                 return realDistamce
#         return 0
#     except Exception as e:
#         print(e)
#         return 0