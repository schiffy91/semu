"""Centralised dispatcher for filesystem and subprocess operations.

Honors the global DRY_RUN flag; logs every action; counts errors.
"""

import os
import stat as stat_module
import subprocess
import sys

from core import state
from core.console import console_error, console_log

DRY_RUN = state.DRY_RUN  # re-export for tests


_DISPATCH = {
    "unlink": lambda args, kw: os.unlink(*args),
    "remove": lambda args, kw: os.remove(*args),
    "symlink": lambda args, kw: os.symlink(args[0], args[1], **kw),
    "makedirs": lambda args, kw: os.makedirs(*args, exist_ok=True),
    "run": lambda args, kw: subprocess.run(*args, capture_output=True, text=True, check=False),
    "install": lambda args, kw: subprocess.run(*args, stdin=sys.stdin, stdout=sys.stdout, check=False),
    "chmod": lambda args, kw: os.chmod(*args),
    "rmdir": lambda args, kw: os.rmdir(*args),
}


def execute(name, *args, **kwargs):
    console_log(f"{name}({', '.join(repr(arg) for arg in args)})")
    if state.DRY_RUN:
        return None
    fn = _DISPATCH.get(name)
    if fn is None:
        console_error(f"Unknown execute command: {name}")
        state.add_error()
        return None
    try:
        return fn(args, kwargs)
    except Exception as e:
        console_error(f"Failed to execute {name}({', '.join(repr(arg) for arg in args)})\n{e}\n")
        state.add_error()
        return None


def rmtree(path):
    """Remove a directory tree without following symlinks."""
    for root, dirs, files in os.walk(path, topdown=False, followlinks=False):
        for name in files:
            filename = os.path.join(root, name)
            if os.path.islink(filename):
                execute("unlink", filename)
            else:
                execute("chmod", filename, stat_module.S_IRWXU)
                execute("remove", filename)
        for name in dirs:
            dirname = os.path.join(root, name)
            if os.path.islink(dirname):
                execute("unlink", dirname)
            else:
                execute("chmod", dirname, stat_module.S_IRWXU)
                execute("rmdir", dirname)
    execute("rmdir", path)


def delete(path):
    if os.path.islink(path):
        execute("unlink", path)
    elif os.path.isdir(path):
        rmtree(path)
    elif os.path.exists(path):
        execute("remove", path)


def own(path):
    execute("chmod", path, stat_module.S_IRWXU)
