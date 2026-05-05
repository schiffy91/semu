"""Console output helpers. Kept tiny so the GUI can replace these by overriding
the module-level functions to capture output into a Qt log panel."""


def console_error(message):
    print(f"\n\033[91m{message}\n\033[0m")


def console_log(message):
    print(f"{message}")
