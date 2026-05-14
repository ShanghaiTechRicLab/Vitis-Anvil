"""anvil.log — loguru wrapper mirroring anvil::log C++ API."""
from __future__ import annotations

import os
import sys

from loguru import logger

_initialized = False
_current_tag = "anvil"


def _format() -> str:
    return "[{time:YYYY-MM-DDTHH:mm:ss}] [{level}] [{extra[tag]}] {message}"


def init(tag: str = "anvil") -> None:
    global _initialized, _current_tag
    _current_tag = tag
    if _initialized:
        logger.configure(extra={"tag": tag})
        return
    logger.remove()
    level = os.environ.get("ANVIL_LOG_LEVEL", "INFO").upper()
    logger.add(sys.stderr, format=_format(), level=level)
    logger.configure(extra={"tag": tag})
    _initialized = True


def set_level(level: str) -> None:
    logger.remove()
    logger.add(sys.stderr, format=_format(), level=level.upper())
    logger.configure(extra={"tag": _current_tag})


def trace(msg: str, *args, **kwargs) -> None:
    logger.opt(depth=1).trace(msg, *args, **kwargs)


def debug(msg: str, *args, **kwargs) -> None:
    logger.opt(depth=1).debug(msg, *args, **kwargs)


def info(msg: str, *args, **kwargs) -> None:
    logger.opt(depth=1).info(msg, *args, **kwargs)


def warn(msg: str, *args, **kwargs) -> None:
    logger.opt(depth=1).warning(msg, *args, **kwargs)


def error(msg: str, *args, **kwargs) -> None:
    logger.opt(depth=1).error(msg, *args, **kwargs)
