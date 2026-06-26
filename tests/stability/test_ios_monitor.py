import unittest
from pathlib import Path


STABILITY_DIR = Path(__file__).resolve().parent
IOS_MONITOR = STABILITY_DIR / "platforms" / "ios" / "monitor.sh"


class IOSMonitorTest(unittest.TestCase):
    def test_dead_process_branch_checks_done_marker_before_crash(self):
        script = IOS_MONITOR.read_text()

        self.assertIn(
            'DONE_CONTENT=""',
            script,
        )
        self.assertIn(
            'DONE_FILE="${CONTAINER_PATH}/Documents/stability/stability_done.txt"',
            script,
        )
        self.assertIn(
            'echo "[Monitor] App completed gracefully before process exit: ${DONE_CONTENT}"',
            script,
        )

    def test_alive_process_branch_checks_done_marker_before_freeze(self):
        script = IOS_MONITOR.read_text()

        self.assertIn(
            '# Process is alive — check for graceful completion before freeze detection',
            script,
        )
        self.assertIn(
            'echo "[Monitor] App completed gracefully while process is still alive: ${DONE_CONTENT}"',
            script,
        )


if __name__ == "__main__":
    unittest.main()
