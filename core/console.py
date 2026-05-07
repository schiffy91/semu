"""Console output helpers.

Both `console_log` and `console_error` print to stdout (so the GUI's worker
thread can capture them as a log stream) AND emit through stdlib logging
(so file logs include the same content with timestamps + levels).
"""

from core.logger import get_logger


def console_error(message):
    print(f"\n\033[91m{message}\n\033[0m")
    get_logger().error(str(message).strip())


def console_log(message):
    print(f"{message}")
    msg = str(message).strip()
    if msg:
        get_logger().info(msg)
