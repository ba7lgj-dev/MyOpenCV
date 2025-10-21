"""Central logging configuration helpers for the application."""

from __future__ import annotations

import logging
import sys
from typing import Optional

_LOGGING_CONFIGURED = False


def setup_logging(level: int = logging.INFO, *, force: bool = False) -> None:
    """Configure the root logger to output human-readable logs to stdout."""

    global _LOGGING_CONFIGURED
    if _LOGGING_CONFIGURED and not force:
        return

    handler = logging.StreamHandler(sys.stdout)
    handler.setLevel(level)
    formatter = logging.Formatter(
        "%(asctime)s [%(levelname)s] %(name)s: %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )
    handler.setFormatter(formatter)

    root_logger = logging.getLogger()
    root_logger.setLevel(level)
    root_logger.handlers.clear()
    root_logger.addHandler(handler)

    _LOGGING_CONFIGURED = True


def get_logger(name: Optional[str] = None) -> logging.Logger:
    """Return a logger using the shared configuration."""
    if not _LOGGING_CONFIGURED:
        setup_logging()
    return logging.getLogger(name if name else __name__)
