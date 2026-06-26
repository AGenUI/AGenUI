import unittest
from pathlib import Path


STABILITY_DIR = Path(__file__).resolve().parent
IOS_LAUNCH = STABILITY_DIR / "platforms" / "ios" / "launch.sh"


class IOSLaunchTest(unittest.TestCase):
    def test_simulator_cleanup_clears_last_crash_scenario_marker(self):
        script = IOS_LAUNCH.read_text()

        self.assertIn('last_crash_scenario.txt', script)

    def test_simulator_cleanup_clears_done_marker(self):
        script = IOS_LAUNCH.read_text()

        self.assertIn('stability_done.txt', script)


if __name__ == "__main__":
    unittest.main()
