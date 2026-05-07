"""GUI entry point. Run via `python -m gui.app` or `setup.py gui`."""

import os
import sys


def main():
    # Ensure the repo root is on sys.path so `import core` works regardless of CWD.
    sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    try:
        from PySide6.QtWidgets import QApplication
    except ImportError:
        print(
            "PySide6 is not installed. Install with `pip install PySide6` or use "
            "the bundled .app / AppImage release.",
            file=sys.stderr,
        )
        sys.exit(1)

    from gui.main_window import MainWindow

    app = QApplication(sys.argv)
    app.setApplicationName("Schemulator")
    app.setOrganizationName("Schemulator")

    window = MainWindow()
    window.show()

    if "--no-wizard" not in sys.argv:
        # Show the first-run wizard if this looks like a fresh install. Done
        # after `show()` so the wizard appears as a child of the main window.
        try:
            window.maybe_show_first_run_wizard()
        except Exception as e:
            # Wizard failure must not block the app.
            print(f"First-run wizard skipped: {e}", file=sys.stderr)

    sys.exit(app.exec())


if __name__ == "__main__":
    main()
