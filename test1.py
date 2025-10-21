import logging
import time

import cv2
import numpy as np

logging.basicConfig(level=logging.INFO, format="%(asctime)s [%(levelname)s] %(message)s")

WAIT_BETWEEN_IMAGES_MS = 1000

i = 1

while i<=57:
    img = cv2.imread("imgData" + "/a" + str(i) + ".jpg")
    gray_img = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    # 二值化
    # 参数：1.原图像， 2.进行分类的阈值，3.高于（低于）阈值时赋予的新值，4.方法选择参数
    # 返回值：1.得到的阈值，2.阈值化后的图像
    retval, threshold_img = cv2.threshold(gray_img, 30, 255, cv2.THRESH_BINARY)

    cv2.imwrite("result" + "/threshold_img" + str(i) + ".jpg", threshold_img)

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

    cv2.imwrite("result" + "/result_img" + str(i) + ".jpg", result_img)
    logging.info("等待 %d ms 后处理下一张图像", WAIT_BETWEEN_IMAGES_MS)
    time.sleep(WAIT_BETWEEN_IMAGES_MS / 1000.0)
    i = i+1
