import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]
IOS_VIEW_CONTROLLER = (
    REPO_ROOT
    / "playground"
    / "ios"
    / "Playground"
    / "Playground for AGenUI"
    / "Stability"
    / "StabilityTestViewController.swift"
)


class IOSStabilityViewControllerTest(unittest.TestCase):
    def test_stop_test_writes_done_marker(self):
        content = IOS_VIEW_CONTROLLER.read_text()

        self.assertIn("writeDoneMarker(reason: reason)", content)
        self.assertIn('let doneFileURL = stabilityDir.appendingPathComponent("stability_done.txt")', content)


if __name__ == "__main__":
    unittest.main()
