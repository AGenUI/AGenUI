import unittest
from pathlib import Path


STABILITY_DIR = Path(__file__).resolve().parent
ANDROID_MONITOR = STABILITY_DIR / "platforms" / "android" / "monitor.sh"


class AndroidMonitorTest(unittest.TestCase):
    def test_alive_process_branch_still_checks_done_marker(self):
        script = ANDROID_MONITOR.read_text()

        self.assertIn(
            '# Process is alive — check for graceful completion before freeze detection',
            script,
        )
        self.assertIn(
            'DONE_CONTENT=$(adb shell cat "$DEVICE_DONE_FILE" 2>/dev/null | tr -d \'\\r\' || echo "")',
            script,
        )
        self.assertIn(
            'echo "[Monitor] App completed gracefully while process is still alive: ${DONE_CONTENT}"',
            script,
        )


if __name__ == "__main__":
    unittest.main()
