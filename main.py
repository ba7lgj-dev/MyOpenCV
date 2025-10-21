import threading
import time
import tkinter as tk
from tkinter import ttk
from tkinter import messagebox

import numpy as np
import requests
import cv2
import io

from PIL import ImageTk, Image

import camera.camera
from camera.camera import getImage
from utils import http
from utils.mathCount import find_midpoint, find_intersection, distance_between_points


class BasePage(tk.Toplevel):
    # 类属性作为静态属性
    static_url = ""

    # 类方法来获取静态属性
    @classmethod
    def get_static_property(cls):
        return cls.static_url

    # 类方法来设置静态属性
    @classmethod
    def set_static_property(cls, value):
        cls.static_url = value

    # 类属性作为静态属性
    add_url = ""

    # 类方法来获取静态属性
    @classmethod
    def get_addUrl(cls):
        return cls.add_url

    # 类方法来设置静态属性
    @classmethod
    def set_add_url(cls, value):
        cls.add_url = value

    def __init__(self, master=None):
        super().__init__(master)
        self.protocol("WM_DELETE_WINDOW", self.on_close)
        self.geometry("500x400")  # 基础窗口大小
        self.resizable(False, False)  # 禁止调整窗口大小


    def on_close(self):
        # self.master.deiconify()  # 显示主窗口
        self.destroy()  # 关闭当前窗口
        exit()


class HomePage(BasePage):
    def __init__(self, master=None):
        super().__init__(master)
        self.master.withdraw()  # 隐藏主窗口
        self.create_widgets()

    def create_widgets(self):
        ip_label = ttk.Label(self, text="请输入IP地址:")
        ip_label.pack(pady=10)
        self.ip_entry = ttk.Entry(self)
        self.ip_entry.pack(pady=5)
        with open('url.txt', 'r') as file:
            self.ip_entry.insert(0, file.readline())
            print(f'监控地址{BasePage.get_static_property()}')

            file.close()


        test_button = ttk.Button(self, text="测试连接", command=self.test_connection)
        test_button.pack(pady=10)

        self.info_label = ttk.Label(self, text="", wraplength=200)
        self.info_label.pack(pady=10)

    def test_connection(self):
        ip = self.ip_entry.get()
        BasePage.set_static_property(f"http://{ip}")  # 假设的测试API
        try:
            response = requests.get(BasePage.get_static_property() + "/capture", timeout=5)
            if response.status_code == 200:
                self.info_label.config(text="连接成功！")
                self.after(1000, self.open_second_page)
            else:
                self.info_label.config(text=f"连接失败，状态码：{response.status_code}")
            response.close()
        except requests.exceptions.RequestException as e:
            self.info_label.config(text=f"连接错误：{e}")


    def open_second_page(self):
        self.withdraw()
        SecondPage(self)


class SecondPage(BasePage):
    def __init__(self, master=None):
        super().__init__(master)
        self.create_widgets()
        self.start()


        camera.camera.init(BasePage.get_static_property())

    def create_widgets(self):
        self.canvas = tk.Canvas(self, width=int(self.winfo_screenwidth()), height=200)
        self.canvas.pack()

        self.real_length_entry = ttk.Entry(self)
        self.real_length_entry.pack(pady=5)

        calibrate_button = ttk.Button(self, text="矫正", command=self.open_third_page)
        calibrate_button.pack(pady=10)

    def start(self):
        self.insert_thread = threading.Thread(target=self.update_image, daemon=True)
        self.insert_thread.start()

    def update_image(self):
        self.update_image_run = True

        while self.update_image_run:

            try:
                img,width = getImage(BasePage.get_static_property())
                height = int((img.shape[0] / img.shape[1]) * (self.winfo_screenwidth() / 2))  # 计算等比高度
                img = cv2.resize(img, (int(self.winfo_screenwidth() / 2), height))

                img = Image.fromarray(img)
                imgtk = ImageTk.PhotoImage(image=img)
                self.canvas.imgtk = imgtk  # 保持图像的引用以防止垃圾回收
                self.canvas.create_image(0, 0, anchor=tk.NW, image=imgtk)
                threading.Event().wait(3)
            except:
                print("请调整图像参数")
                pass


    def open_third_page(self):

        _, length = getImage(BasePage.get_static_property())
        rate = float(self.real_length_entry.get()) / float(length)
        self.update_image_run = False
        # self.withdraw()
        self.insert_thread
        ThirdPage(self, rate=rate)


class ThirdPage(BasePage):
    def __init__(self, master=None, rate=float):
        super().__init__(master)
        self.create_widgets()
        self.rate = rate
        self.trigger_count = 0  # 新增触发计数器
        self.inflate_button = None  # 加气按钮引用
        self.start()
        with open('url.txt', 'r') as file:
            file.readline()
            self.add_url = file.readline()
            print(f'监控地址{self.add_url}')

            file.close()

    def start(self):
        data = threading.Thread(target=self.update_data, daemon=True)
        data.start()

    def update_data(self):
        while True:
            threading.Event().wait(0.5)
            _, length = getImage(BasePage.get_static_property())
            if length == 0:
                continue

            # 更新GUI数据

            self.text_area.delete(1.0, tk.END)
            trueLength = length * self.rate
            self.text_area.insert(tk.END, str(f"{trueLength:.2f}mm"))

            # 监控逻辑
            if self.is_monitoring:
                if float(length * self.rate) < float(self.input_box.get()):
                    self.trigger_count += 1
                    self.send_wechat_alert(length)

                    # 触发3次后显示加气按钮
                    if self.trigger_count >= 3 and not self.inflate_button:
                        self.show_inflate_button()
                        self.handle_inflate()
                        webhook_url = "https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=1ffec59d-3ef7-4fc7-939f-2c69dd0d7aa6"
                        message = f"执行加气{trueLength:.2f}mm"
                        http.send_wechat_work_message(webhook_url, message)

    def send_wechat_alert(self, length):
        """发送企业微信提醒（保留原有逻辑）"""
        webhook_url = "https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=b3b26998-1042-472e-af7d-2b0649233be6"
        message = f"宽度提醒：当前宽度{length:.2f}mm低于阈值"
        # http.send_wechat_work_message(webhook_url, message)

    def show_inflate_button(self):
        """显示加气按钮"""
        if not self.inflate_button:
            # 创建加气按钮（浅红色背景）
            self.inflate_button = tk.Button(
                self,
                text="加气",
                bg="#FFB6C1",  # 浅红色
                fg="white",
                font=("Arial", 14),
                command=self.handle_inflate,
                width=15,
                height=2
            )


            # self.monitor_button.pack(pady=5)

            self.inflate_button.pack(pady=6, padx=10)
            # self.inflate_button.place(relx=0.5, rely=0.7, anchor="center")  # 页面底部居中


    def handle_inflate(self):
        """处理加气操作"""
        # 禁用按钮防止重复点击
        self.inflate_button.config(state=tk.DISABLED)

        # 使用线程避免GUI冻结
        def send_request():
            try:
                response = requests.get(f"http://{self.add_url}/control?pin=D1&duration=500")  # 替换为实际地址
                if response.status_code == 200:
                    self.reset_trigger_count()
                else:
                    self.show_error("加气失败，服务器返回错误")
            except Exception as e:
                self.show_error(f"请求失败：{str(e)}")
            finally:
                self.inflate_button = None


        threading.Thread(target=send_request).start()

        self.inflate_button.config(text="等待10秒震荡中")

        time.sleep(10)

    def reset_trigger_count(self):
        """重置触发计数和界面状态"""
        self.trigger_count = 0
        if self.inflate_button:
            self.inflate_button.destroy()
            self.inflate_button = None
        # 这里可以添加加气成功后的其他逻辑

    def show_error(self, message):
        """显示错误提示"""
        error_label = tk.Label(
            self,
            text=message,
            fg="red",
            bg="yellow"
        )
        error_label.pack(pady=10)
        self.master.after(3000, lambda: error_label.destroy())  # 3秒后自动消失

    def create_widgets(self):
        """创建界面组件"""
        self.text_area = tk.Text(self, height=1, width=30,font=("Arial",65))
        self.text_area.pack(pady=10, padx=10)

        back_button = ttk.Button(self, text="返回", command=self.return_home)
        back_button.pack(pady=10)

        self.is_monitoring = False

        # 创建输入框
        self.input_box = tk.Entry(self)
        self.input_box.pack(pady=10)

        # 创建监控按钮
        self.monitor_button = tk.Button(self, text="监控", command=self.toggle_monitoring)
        self.monitor_button.pack(pady=5)

    def toggle_monitoring(self):
        """切换监控状态"""
        self.is_monitoring = not self.is_monitoring
        button_text = "取消监控" if self.is_monitoring else "开始监控"
        self.monitor_button.config(text=button_text)
        # 重置触发计数当切换为监控状态时
        if self.is_monitoring:
            self.trigger_count = 0

    def return_home(self):
        """返回首页"""
        HomePage(self)

if __name__ == "__main__":
    root = tk.Tk()
    app = HomePage(root)
    root.mainloop()


def showImg(image):
    height, width = image.shape

    # 设定最大宽度和最大高度
    max_dimension = 800  # 最大宽度或高度为800像素

    # 确定缩放比例
    if width > height:
        scale = max_dimension / width
    else:
        scale = max_dimension / height

    # 计算缩小后的宽度和高度
    new_width = int(width * scale)
    new_height = int(height * scale)

    # 缩小图片
    resized_image = cv2.resize(image, (new_width, new_height), interpolation=cv2.INTER_AREA)

    cv2.imshow('Image', resized_image)
    cv2.waitKey()
    # cv2.destroyAllWindows()

#
# if __name__ == "__main__":
#     i = 1
#     while True:
#         # img = http.get_image_from_url("http://192.168.137.208/capture")
#         # cv2.imwrite("imgData" + "/a"+str(i)+".jpg", img)
#
#         img = cv2.imread("imgData" + "/a" + str(i) + ".jpg")
#
#         i = i + 1
#
#
#
#         # 灰度图
#         img = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
#         img = cv2.bilateralFilter(img, d=5, sigmaColor=75, sigmaSpace=75)
#
#         copy_img = img
#
#         _, img = cv2.threshold(img, 80, 255, cv2.THRESH_BINARY)
#
#         kernel = np.ones((20, 20), np.uint8)
#         img = cv2.morphologyEx(img, cv2.MORPH_OPEN, kernel)
#
#         # 查找轮廓
#         contours, _ = cv2.findContours(img, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
#
#         try:
#             img_with_contours = copy_img
#
#             # 遍历轮廓并绘制
#             for contour in contours:
#                 # rect = cv2.minAreaRect(contour)
#                 #
#                 # # 获取最小外接矩形的中心点、尺寸和旋转角度
#                 # center, size, angle = rect
#                 #
#                 # # 计算长边的长度作为两“竖线”间的理想距离，因为我们无法直接画出并测量垂线，这是近似方法
#                 # parallel_side_length = max(size)  # 假设长边代表两竖线间距离的参考值
#                 #
#                 # print(f"{parallel_side_length}px")
#                 #
#                 # # 如果需要展示这个过程中的辅助图形，可以使用以下代码绘制最小外接矩形
#                 # box = cv2.boxPoints(rect)
#                 # box = np.intp(box)
#                 # img_with_contours = cv2.drawContours(img_with_contours, [box], 0, (0, 99, 255), 2)
#                 epsilon = 0.1 * cv2.arcLength(contour, True)
#                 approx = cv2.approxPolyDP(contour, epsilon, True)
#
#                 # # 绘制轮廓
#                 if len(approx) == 4:
#                     cv2.drawContours(img_with_contours, [approx], 0, (0, 255, 0), 2)  # 绘制矩形轮廓
#                     #     不太正确的处理
#                     # 分别为左上 左下 右下 右上
#                     pointLT = (approx[0][0][0], approx[0][0][1])
#                     pointLB = (approx[1][0][0], approx[1][0][1])
#                     pointRB = (approx[2][0][0], approx[2][0][1])
#                     pointRT = (approx[3][0][0], approx[3][0][1])
#
#                     # 中间值 没啥用
#                     LLinemiddle = find_midpoint(pointLT, pointLB)
#                     RLinemiddle = find_midpoint(pointRB, pointRT)
#
#                     # RLineIntersectionPoint = find_intersection(pointLT, pointLB, pointRT, pointRB)
#
#                     cv2.line(img_with_contours, LLinemiddle, RLinemiddle, (0, 0, 255), 2)
#                     distance = distance_between_points(LLinemiddle, RLinemiddle)
#                     realDistamce = distance
#                     cv2.putText(img_with_contours, f"{realDistamce:.0f}", RLinemiddle,
#                                 cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 1, cv2.LINE_AA)
#                     # img_with_contours = cv2.drawContours(img_with_contours, [contour], -1, (0,0,255), 2)  # 红色绘制轮廓，线宽为2
#                     showImg(img_with_contours)
#         except Exception as e:
#
#             print(e)
#
#         i += 1
#         time.sleep(1)
#         # break
