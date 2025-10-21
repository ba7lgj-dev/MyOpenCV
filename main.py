"""Main application entry point for the measurement UI.

The original script mixed a lot of global state, blocking calls on the UI
thread and ad-hoc thread management.  This refactor introduces a small
application state object that is injected into every page and centralises the
navigation and background work management.  Tkinter widgets are now updated via
``after`` callbacks which keeps the UI responsive while heavy work happens in
threads.
"""

from __future__ import annotations

import dataclasses
import threading
from typing import Optional

import tkinter as tk
from tkinter import ttk, messagebox

import cv2
import numpy as np
import requests
from PIL import Image, ImageTk

import camera.camera
from camera.camera import CameraProcessingError, getImage
from utils import http


CONFIG_FILE = "url.txt"
REFRESH_INTERVAL_SECONDS = 0.5
STREAM_UPDATE_INTERVAL_SECONDS = 3
INFLATE_ENDPOINT = "/control?pin=D1&duration=500"


@dataclasses.dataclass
class AppState:
    """Holds the URLs that are shared across different pages."""

    camera_base_url: str = ""
    inflator_host: str = ""
    inflate_duration_seconds: int = 10

    @property
    def camera_capture_url(self) -> str:
        return self._ensure_http_prefix(self.camera_base_url)

    @property
    def inflator_base_url(self) -> str:
        return self._ensure_http_prefix(self.inflator_host)

    def update_camera_host(self, host: str) -> None:
        self.camera_base_url = host.strip()

    def update_inflator_host(self, host: str) -> None:
        self.inflator_host = host.strip()

    @staticmethod
    def _ensure_http_prefix(url: str) -> str:
        if not url:
            return ""
        if url.startswith("http://") or url.startswith("https://"):
            return url
        return f"http://{url}"

    @classmethod
    def load_from_file(cls, path: str) -> "AppState":
        """Load the persisted addresses from ``url.txt`` if it exists."""

        state = cls()
        try:
            with open(path, "r", encoding="utf-8") as file:
                lines = [line.strip() for line in file.read().splitlines() if line.strip()]
        except FileNotFoundError:
            return state

        if lines:
            state.update_camera_host(lines[0])
        if len(lines) > 1:
            state.update_inflator_host(lines[1])
        if len(lines) > 2:
            try:
                state.inflate_duration_seconds = max(1, int(lines[2]))
            except ValueError:
                state.inflate_duration_seconds = 10
        return state

    def persist(self, path: str) -> None:
        with open(path, "w", encoding="utf-8") as file:
            lines = [
                self.camera_base_url,
                self.inflator_host,
                str(self.inflate_duration_seconds),
            ]
            file.write("\n".join(lines))


class BasePage(tk.Toplevel):
    """Base ``Toplevel`` with shared helpers for all pages."""

    def __init__(self, master: tk.Misc, state: AppState):
        super().__init__(master)
        self.state = state
        self.resizable(True, True)
        self.protocol("WM_DELETE_WINDOW", self.on_close)

    def on_close(self) -> None:
        self.destroy()


class HomePage(BasePage):
    """Initial page where the user can configure the camera host."""

    def __init__(self, master: tk.Misc, state: AppState):
        super().__init__(master, state)
        self.master.withdraw()
        self.title("连接设置")
        self.geometry("400x200")
        self._create_widgets()

    def _create_widgets(self) -> None:
        ttk.Label(self, text="请输入摄像头 IP 地址:").pack(pady=10)

        self.ip_entry = ttk.Entry(self)
        self.ip_entry.pack(pady=5)
        if self.state.camera_base_url:
            # Persisted value is already prefixed with http; keep only the host part.
            host = self.state.camera_base_url.replace("http://", "").replace("https://", "")
            self.ip_entry.insert(0, host)

        ttk.Button(self, text="测试连接", command=self.test_connection).pack(pady=10)

        self.info_label = ttk.Label(self, text="", wraplength=280)
        self.info_label.pack(pady=10)

    def test_connection(self) -> None:
        host = self.ip_entry.get().strip()
        if not host:
            self.info_label.config(text="请输入有效的IP地址")
            return

        self.state.update_camera_host(host)
        try:
            response = requests.get(f"{self.state.camera_capture_url}/capture", timeout=5)
            response.raise_for_status()
        except requests.RequestException as exc:
            self.info_label.config(text=f"连接错误：{exc}")
            return

        self.state.persist(CONFIG_FILE)
        self.info_label.config(text="连接成功！")
        self.after(500, self.open_second_page)

    def open_second_page(self) -> None:
        self.withdraw()
        SecondPage(self.master, self.state, on_return=self._on_child_return)

    def _on_child_return(self) -> None:
        self.deiconify()

    def on_close(self) -> None:
        super().on_close()
        if isinstance(self.master, tk.Tk):
            self.master.destroy()


class SecondPage(BasePage):
    """Displays the live feed and allows the user to calibrate."""

    def __init__(self, master: tk.Misc, state: AppState, on_return):
        super().__init__(master, state)
        self.title("实时监控")
        self.geometry("1024x720")
        self.on_return = on_return
        self.real_length_entry: Optional[ttk.Entry] = None
        self.canvas: Optional[tk.Canvas] = None
        self._image_thread: Optional[threading.Thread] = None
        self._image_stop_event = threading.Event()
        self._latest_frame_width = 0
        self._status_var = tk.StringVar()
        self._zoom_var = tk.DoubleVar(value=1.0)
        self._last_frame: Optional[np.ndarray] = None

        self._create_widgets()
        self._start_stream()
        self._init_camera()

    def _create_widgets(self) -> None:
        main_frame = ttk.Frame(self)
        main_frame.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

        canvas_width = int(self.winfo_screenwidth() * 0.55)
        self.canvas = tk.Canvas(main_frame, width=canvas_width, height=360, background="#1c1c1c")
        self.canvas.grid(row=0, column=0, sticky="nsew")

        controls_frame = ttk.Frame(main_frame)
        controls_frame.grid(row=1, column=0, sticky="ew", pady=(10, 0))

        ttk.Label(controls_frame, text="缩放").grid(row=0, column=0, padx=5)
        zoom_scale = ttk.Scale(
            controls_frame,
            from_=0.5,
            to=2.0,
            variable=self._zoom_var,
            orient=tk.HORIZONTAL,
            command=lambda _event=None: self._redraw_last_image(),
        )
        zoom_scale.grid(row=0, column=1, padx=5, sticky="ew")
        controls_frame.columnconfigure(1, weight=1)

        self.real_length_entry = ttk.Entry(controls_frame)
        self.real_length_entry.grid(row=0, column=2, padx=10)
        self.real_length_entry.insert(0, "请输入已知长度(mm)")

        ttk.Button(controls_frame, text="矫正", command=self.open_third_page).grid(row=0, column=3, padx=10)

        ttk.Label(
            main_frame,
            textvariable=self._status_var,
            foreground="red",
        ).grid(row=2, column=0, columnspan=1, pady=(8, 0), sticky="w")

        main_frame.rowconfigure(0, weight=1)
        main_frame.columnconfigure(0, weight=1)

    def _init_camera(self) -> None:
        if not self.state.camera_capture_url:
            self._status_var.set("未配置摄像头地址")
            return

        status = camera.camera.init(self.state.camera_capture_url)
        details = [f"{key}:{value}" for key, value in status.items()]
        self._status_var.set(" | ".join(details))

    def _start_stream(self) -> None:
        self._image_stop_event.clear()
        self._image_thread = threading.Thread(target=self._update_image_loop, daemon=True)
        self._image_thread.start()

    def _update_image_loop(self) -> None:
        while not self._image_stop_event.is_set():
            if not self.state.camera_capture_url:
                self.after(0, lambda: self._status_var.set("未配置摄像头地址"))
                self._image_stop_event.wait(STREAM_UPDATE_INTERVAL_SECONDS)
                continue

            try:
                frame, width = getImage(self.state.camera_capture_url)
            except CameraProcessingError as exc:
                self.after(0, lambda e=exc: self._status_var.set(f"请调整图像参数: {e}"))
                self._image_stop_event.wait(STREAM_UPDATE_INTERVAL_SECONDS)
                continue
            except Exception as exc:  # noqa: BLE001 - unexpected but keep the loop alive
                self.after(0, lambda e=exc: self._status_var.set(f"未知错误: {e}"))
                self._image_stop_event.wait(STREAM_UPDATE_INTERVAL_SECONDS)
                continue

            self._latest_frame_width = width
            self._last_frame = frame
            photo = self._convert_to_photo_image(frame)
            self.after(0, lambda p=photo: self._draw_image(p))
            self._image_stop_event.wait(STREAM_UPDATE_INTERVAL_SECONDS)

    def _convert_to_photo_image(self, frame: np.ndarray) -> ImageTk.PhotoImage:
        frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        zoom = max(0.5, min(2.0, self._zoom_var.get()))
        canvas_width = int(self.winfo_screenwidth() * 0.55 * zoom)
        height = int(frame_rgb.shape[0] / frame_rgb.shape[1] * canvas_width)
        resized = cv2.resize(frame_rgb, (canvas_width, height))
        image = Image.fromarray(resized)
        return ImageTk.PhotoImage(image=image)

    def _draw_image(self, photo: ImageTk.PhotoImage) -> None:
        if not self.canvas:
            return
        self.canvas.imgtk = photo  # keep reference
        self.canvas.delete("all")
        self.canvas.config(width=photo.width(), height=photo.height())
        self.canvas.create_image(0, 0, anchor=tk.NW, image=photo)

    def _redraw_last_image(self) -> None:
        if self._last_frame is None:
            return
        photo = self._convert_to_photo_image(self._last_frame)
        self._draw_image(photo)

    def open_third_page(self) -> None:
        if not self.real_length_entry:
            return
        try:
            real_length = float(self.real_length_entry.get())
        except ValueError:
            messagebox.showerror("输入错误", "请输入正确的数字长度")
            return

        try:
            _, pixel_length = getImage(self.state.camera_capture_url)
        except CameraProcessingError as exc:
            messagebox.showerror("测量失败", f"无法获取图像: {exc}")
            return
        except Exception as exc:  # noqa: BLE001 - keep behaviour predictable for unknown issues
            messagebox.showerror("测量失败", f"未知错误: {exc}")
            return

        if pixel_length == 0:
            messagebox.showerror("测量失败", "未检测到可用于矫正的白色区域")
            return

        rate = real_length / pixel_length
        self._image_stop_event.set()
        self.withdraw()
        ThirdPage(self, self.state, rate, on_return=self._on_third_page_return)

    def _on_third_page_return(self) -> None:
        self.deiconify()
        self._start_stream()

    def on_close(self) -> None:
        self._image_stop_event.set()
        super().on_close()
        if self.on_return:
            self.on_return()


class ThirdPage(BasePage):
    """Page that shows the measured width in millimetres and monitoring logic."""

    def __init__(self, master: tk.Misc, state: AppState, rate: float, on_return):
        super().__init__(master, state)
        self.title("宽度监控")
        self.geometry("600x400")
        self.rate = rate
        self.on_return = on_return

        self.measurement_var = tk.StringVar(value="-- mm")
        self.status_var = tk.StringVar(value="")
        self.threshold: Optional[float] = None
        self.is_monitoring = False
        self.trigger_count = 0
        self.inflate_button: Optional[tk.Button] = None
        self._stop_event = threading.Event()
        self._monitor_thread = threading.Thread(target=self._update_data_loop, daemon=True)

        self._create_widgets()
        self._monitor_thread.start()
        self._load_inflator_host()

    def _create_widgets(self) -> None:
        ttk.Label(self, textvariable=self.measurement_var, font=("Arial", 48)).pack(pady=20)

        ttk.Label(self, textvariable=self.status_var, foreground="red").pack()

        controls_frame = ttk.Frame(self)
        controls_frame.pack(pady=10)

        ttk.Label(controls_frame, text="报警阈值(mm)").grid(row=0, column=0, padx=5)
        self.threshold_entry = ttk.Entry(controls_frame)
        self.threshold_entry.grid(row=0, column=1, padx=5)

        self.monitor_button = ttk.Button(controls_frame, text="开始监控", command=self.toggle_monitoring)
        self.monitor_button.grid(row=0, column=2, padx=5)

        ttk.Label(controls_frame, text="加气时长(s)").grid(row=1, column=0, padx=5, pady=(8, 0))
        self.inflate_duration_entry = ttk.Entry(controls_frame)
        self.inflate_duration_entry.grid(row=1, column=1, padx=5, pady=(8, 0))
        self.inflate_duration_entry.insert(0, str(self.state.inflate_duration_seconds))
        ttk.Button(controls_frame, text="应用", command=self._save_inflate_duration).grid(row=1, column=2, padx=5, pady=(8, 0))

        inflator_frame = ttk.Frame(self)
        inflator_frame.pack(pady=10)
        ttk.Label(inflator_frame, text="加气主机地址").grid(row=0, column=0, padx=5)
        self.inflator_entry = ttk.Entry(inflator_frame, width=25)
        self.inflator_entry.grid(row=0, column=1, padx=5)
        ttk.Button(inflator_frame, text="保存", command=self._save_inflator_host).grid(row=0, column=2, padx=5)

        ttk.Button(self, text="返回", command=self.return_home).pack(pady=10)

    def _load_inflator_host(self) -> None:
        if not self.state.inflator_host:
            return
        self.status_var.set(f"加气主机: {self.state.inflator_base_url}")
        self.inflator_entry.delete(0, tk.END)
        self.inflator_entry.insert(0, self.state.inflator_host)

    def _save_inflator_host(self) -> None:
        host = self.inflator_entry.get().strip()
        if not host:
            messagebox.showerror("输入错误", "请输入正确的加气主机地址")
            return

        self.state.update_inflator_host(host)
        self.state.persist(CONFIG_FILE)
        self.status_var.set(f"已保存加气主机: {self.state.inflator_base_url}")

    def _save_inflate_duration(self) -> None:
        value = self.inflate_duration_entry.get().strip()
        try:
            seconds = max(1, int(value))
        except ValueError:
            messagebox.showerror("输入错误", "加气时长必须是数字")
            return

        self.state.inflate_duration_seconds = seconds
        self.state.persist(CONFIG_FILE)
        self.status_var.set(f"已保存加气时长: {seconds}s")

    def _update_data_loop(self) -> None:
        while not self._stop_event.is_set():
            if not self.state.camera_capture_url:
                self.after(0, lambda: self.status_var.set("未配置摄像头地址"))
                self._stop_event.wait(REFRESH_INTERVAL_SECONDS)
                continue

            try:
                _, length_px = getImage(self.state.camera_capture_url)
            except CameraProcessingError as exc:
                self.after(0, lambda e=exc: self.status_var.set(f"图像采集失败: {e}"))
                self._stop_event.wait(REFRESH_INTERVAL_SECONDS)
                continue
            except Exception as exc:  # noqa: BLE001 - keep monitoring alive on unexpected errors
                self.after(0, lambda e=exc: self.status_var.set(f"未知错误: {e}"))
                self._stop_event.wait(REFRESH_INTERVAL_SECONDS)
                continue

            if length_px == 0:
                self.after(0, lambda: self.status_var.set("未检测到白色区域"))
                self._stop_event.wait(REFRESH_INTERVAL_SECONDS)
                continue

            length_mm = length_px * self.rate
            self.after(0, lambda mm=length_mm, px=length_px: self._handle_measurement(mm, px))
            self._stop_event.wait(REFRESH_INTERVAL_SECONDS)

    def _handle_measurement(self, length_mm: float, length_px: int) -> None:
        self.measurement_var.set(f"{length_mm:.2f} mm")
        self.status_var.set("")

        if not self.is_monitoring or self.threshold is None:
            return

        if length_mm < self.threshold:
            self.trigger_count += 1
            self.send_wechat_alert(length_mm)
            if self.trigger_count >= 3:
                self._trigger_inflate(length_mm)
        else:
            self.trigger_count = 0

    def send_wechat_alert(self, length_mm: float) -> None:
        webhook_url = "https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=b3b26998-1042-472e-af7d-2b0649233be6"
        message = f"宽度提醒：当前宽度 {length_mm:.2f}mm 低于阈值"
        # http.send_wechat_work_message(webhook_url, message)

    def _trigger_inflate(self, length_mm: float) -> None:
        if not self.inflate_button:
            self.inflate_button = tk.Button(
                self,
                text="加气",
                bg="#FFB6C1",
                fg="white",
                font=("Arial", 14),
                command=self.handle_inflate,
                width=15,
                height=2,
            )
            self.inflate_button.pack(pady=6, padx=10)

        if self.inflate_button["state"] != tk.DISABLED:
            self.handle_inflate()
            webhook_url = "https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=1ffec59d-3ef7-4fc7-939f-2c69dd0d7aa6"
            message = f"执行加气 {length_mm:.2f}mm"
            http.send_wechat_work_message(webhook_url, message)

    def handle_inflate(self) -> None:
        if not self.inflate_button:
            return

        base_url = self.state.inflator_base_url
        if not base_url:
            self.status_var.set("未配置加气主机地址")
            self.inflate_button.config(state=tk.NORMAL, text="加气")
            return

        self.inflate_button.config(state=tk.DISABLED, text="等待中...")

        def request_inflate() -> None:
            url = f"{base_url}{INFLATE_ENDPOINT}"
            try:
                response = requests.get(url, timeout=10)
                response.raise_for_status()
            except requests.RequestException as exc:
                self.after(0, lambda e=exc: self._on_inflate_error(e))
                return

            self.after(0, self._on_inflate_success)

        threading.Thread(target=request_inflate, daemon=True).start()

    def _on_inflate_success(self) -> None:
        self.status_var.set("加气成功，等待震荡稳定...")
        self.trigger_count = 0
        self.after(self.state.inflate_duration_seconds * 1000, self._remove_inflate_button)

    def _on_inflate_error(self, exc: Exception) -> None:
        self.status_var.set(f"加气失败：{exc}")
        if self.inflate_button:
            self.inflate_button.config(state=tk.NORMAL, text="加气")

    def _remove_inflate_button(self) -> None:
        if self.inflate_button:
            self.inflate_button.destroy()
            self.inflate_button = None
        self.status_var.set("")

    def toggle_monitoring(self) -> None:
        if not self.is_monitoring:
            try:
                self.threshold = float(self.threshold_entry.get())
            except ValueError:
                messagebox.showerror("输入错误", "请输入正确的阈值")
                self.threshold = None
                return

            self.trigger_count = 0
            self.is_monitoring = True
            self.monitor_button.config(text="取消监控")
            self.status_var.set("")
        else:
            self.is_monitoring = False
            self.monitor_button.config(text="开始监控")
            self.threshold = None
            self.trigger_count = 0
            self.status_var.set("")

    def return_home(self) -> None:
        self._stop_event.set()
        self.destroy()
        if self.on_return:
            self.on_return()

    def on_close(self) -> None:
        self._stop_event.set()
        super().on_close()
        if self.on_return:
            self.on_return()


def main() -> None:
    root = tk.Tk()
    root.withdraw()
    state = AppState.load_from_file(CONFIG_FILE)
    HomePage(root, state)
    root.mainloop()


if __name__ == "__main__":
    main()
