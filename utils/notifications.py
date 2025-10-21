"""Notification helpers that centralise rate limiting and escalation logic."""

from __future__ import annotations

import json
import threading
import time
from collections import deque
from typing import Deque, Dict

import requests

ALERT_WEBHOOK = "https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=b3b26998-1042-472e-af7d-2b0649233be6"
OPERATIONS_WEBHOOK = "https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=1ffec59d-3ef7-4fc7-939f-2c69dd0d7aa6"


class NotificationManager:
    """Send WeChat notifications with rate limiting and recovery tracking."""

    def __init__(self, max_messages_per_period: int = 20, period_seconds: int = 60) -> None:
        self.max_messages_per_period = max_messages_per_period
        self.period_seconds = period_seconds
        self._lock = threading.Lock()
        self._sent_times: Deque[float] = deque()
        self._error_state: Dict[str, Dict[str, int | bool]] = {}

    def send_text(self, webhook_url: str, message: str) -> bool:
        if not webhook_url or not message:
            return False
        with self._lock:
            return self._send_locked(webhook_url, self._truncate_message(message))

    def notify_info(self, category: str, webhook_url: str, message: str) -> bool:
        return self._send_with_category(category, webhook_url, message)

    def notify_error(
        self,
        category: str,
        webhook_url: str,
        base_message: str,
        *,
        escalate_after: int = 3,
    ) -> bool:
        base_message = self._truncate_message(base_message)
        with self._lock:
            state = self._error_state.setdefault(category, {"count": 0, "active": False})
            state["count"] = int(state["count"]) + 1
            state["active"] = True

            count = int(state["count"])
            if count == 1:
                message = base_message
            elif escalate_after > 0 and count == escalate_after:
                message = f"{base_message}（连续{count}次，请尽快排查）"
            else:
                return False

            return self._send_locked(webhook_url, message)

    def notify_recovery(self, category: str, webhook_url: str, message: str) -> bool:
        with self._lock:
            state = self._error_state.get(category)
            if not state or not state.get("active"):
                return False
            state["count"] = 0
            state["active"] = False
            message = self._truncate_message(message)
            return self._send_locked(webhook_url, message)

    def _send_with_category(self, category: str, webhook_url: str, message: str) -> bool:
        with self._lock:
            return self._send_locked(webhook_url, self._truncate_message(message))

    def _send_locked(self, webhook_url: str, message: str) -> bool:
        if not webhook_url or not message:
            return False
        if not self._can_send_locked():
            print("通知发送频率已达上限，消息被丢弃")
            return False
        payload = {
            "msgtype": "text",
            "text": {"content": message},
        }
        try:
            response = requests.post(
                webhook_url,
                headers={"Content-Type": "application/json"},
                data=json.dumps(payload),
                timeout=5,
            )
            response.raise_for_status()
        except requests.RequestException:
            return False
        self._record_send_locked()
        return True

    def _can_send_locked(self) -> bool:
        now = time.time()
        while self._sent_times and now - self._sent_times[0] > self.period_seconds:
            self._sent_times.popleft()
        return len(self._sent_times) < self.max_messages_per_period

    def _record_send_locked(self) -> None:
        self._sent_times.append(time.time())

    @staticmethod
    def _truncate_message(message: str, max_length: int = 180) -> str:
        message = message.strip()
        if len(message) <= max_length:
            return message
        return f"{message[:max_length - 3]}..."


notifier = NotificationManager()
