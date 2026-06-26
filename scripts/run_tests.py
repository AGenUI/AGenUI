#!/usr/bin/env python3
"""
AGenUI 全平台单元测试运行脚本

支持平台：C++, Android, iOS, HarmonyOS
用法：
    python3 scripts/run_tests.py              # 运行全部平台
    python3 scripts/run_tests.py cpp          # 只运行 C++
    python3 scripts/run_tests.py android      # 只运行 Android
    python3 scripts/run_tests.py ios          # 只运行 iOS
    python3 scripts/run_tests.py harmony      # 只运行 HarmonyOS
    python3 scripts/run_tests.py cpp android  # 运行多个平台
"""

import argparse
import os
import subprocess
import sys
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional


# ─── 配置 ───────────────────────────────────────────────────────────────────

REPO_ROOT = Path(__file__).resolve().parent.parent

# C++ 配置
CPP_SOURCE_DIR = REPO_ROOT / "tests" / "cpp"
CPP_BUILD_DIR = CPP_SOURCE_DIR / "build"

# Android 配置
ANDROID_DIR = REPO_ROOT / "platforms" / "android"

# iOS 配置
IOS_WORKSPACE_DIR = REPO_ROOT / "playground" / "ios" / "Playground"
IOS_WORKSPACE = IOS_WORKSPACE_DIR / "Playground.xcworkspace"
IOS_SCHEME = "Playground"
IOS_DESTINATION = "platform=iOS Simulator,name=iPhone 16"

# HarmonyOS 配置
HARMONY_DIR = REPO_ROOT / "playground" / "harmony"


# ─── 数据结构 ────────────────────────────────────────────────────────────────

@dataclass
class TestResult:
    platform: str
    success: bool
    duration: float = 0.0
    test_count: Optional[int] = None
    failed_count: Optional[int] = None
    output: str = ""
    error: str = ""


# ─── 工具函数 ────────────────────────────────────────────────────────────────

def print_header(title: str):
    width = 70
    print("\n" + "=" * width)
    print(f"  {title}")
    print("=" * width)


def print_result(result: TestResult):
    status = "✅ PASS" if result.success else "❌ FAIL"
    info = f"  [{result.platform}] {status} ({result.duration:.1f}s)"
    if result.test_count is not None:
        info += f" — {result.test_count} tests"
        if result.failed_count:
            info += f", {result.failed_count} failed"
    print(info)


def run_command(
    cmd: list[str],
    cwd: Optional[Path] = None,
    env: Optional[dict] = None,
    timeout: int = 600,
) -> tuple[int, str, str]:
    """执行命令并返回 (returncode, stdout, stderr)"""
    merged_env = os.environ.copy()
    if env:
        merged_env.update(env)

    print(f"  $ {' '.join(cmd)}")
    try:
        proc = subprocess.run(
            cmd,
            cwd=cwd,
            env=merged_env,
            capture_output=True,
            text=True,
            timeout=timeout,
        )
        return proc.returncode, proc.stdout, proc.stderr
    except subprocess.TimeoutExpired:
        return -1, "", f"Command timed out after {timeout}s"
    except FileNotFoundError as e:
        return -1, "", f"Command not found: {e}"


def check_tool(tool: str) -> bool:
    """检查命令行工具是否可用"""
    try:
        subprocess.run(
            [tool, "--version"],
            capture_output=True,
            timeout=10,
        )
        return True
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return False


# ─── C++ 测试 ────────────────────────────────────────────────────────────────

def run_cpp_tests() -> TestResult:
    print_header("C++ Unit Tests (GTest)")
    start = time.time()

    # 1. Configure
    print("\n  [1/3] CMake Configure...")
    rc, out, err = run_command(
        ["cmake", "-S", str(CPP_SOURCE_DIR), "-B", str(CPP_BUILD_DIR)],
        cwd=REPO_ROOT,
    )
    if rc != 0:
        return TestResult("C++", False, time.time() - start, error=err)

    # 2. Build
    cpu_count = os.cpu_count() or 4
    print(f"\n  [2/3] CMake Build (-j{cpu_count})...")
    rc, out, err = run_command(
        ["cmake", "--build", str(CPP_BUILD_DIR), "-j", str(cpu_count)],
        cwd=REPO_ROOT,
        timeout=300,
    )
    if rc != 0:
        return TestResult("C++", False, time.time() - start, error=err)

    # 3. Run tests
    print("\n  [3/3] CTest Run...")
    rc, out, err = run_command(
        ["ctest", "--test-dir", str(CPP_BUILD_DIR), "--output-on-failure"],
        cwd=REPO_ROOT,
        timeout=300,
    )

    # Parse results from ctest output
    test_count = None
    failed_count = None
    for line in out.splitlines():
        # e.g. "99% tests passed, 1 tests failed out of 1102"
        if "tests passed" in line and "out of" in line:
            parts = line.split()
            for i, p in enumerate(parts):
                if p == "out" and i + 2 < len(parts):
                    try:
                        test_count = int(parts[i + 2])
                    except ValueError:
                        pass
                if p == "failed" and i >= 1:
                    try:
                        failed_count = int(parts[i - 1].rstrip(","))
                    except ValueError:
                        pass

    duration = time.time() - start
    return TestResult(
        "C++",
        success=(rc == 0),
        duration=duration,
        test_count=test_count,
        failed_count=failed_count,
        output=out,
        error=err,
    )


# ─── Android 测试 ────────────────────────────────────────────────────────────

def run_android_tests() -> TestResult:
    print_header("Android Unit Tests (JUnit + Mockito)")
    start = time.time()

    gradlew = ANDROID_DIR / "gradlew"
    if not gradlew.exists():
        return TestResult(
            "Android", False, 0,
            error=f"gradlew not found at {gradlew}"
        )

    # Ensure executable
    gradlew.chmod(0o755)

    print("\n  Running ./gradlew testDebugUnitTest...")
    rc, out, err = run_command(
        [str(gradlew), "testDebugUnitTest", "--no-daemon"],
        cwd=ANDROID_DIR,
        timeout=300,
    )

    # Parse test count from Gradle output
    test_count = None
    failed_count = None
    for line in (out + err).splitlines():
        # e.g. "xxx tests completed, yyy failed"
        if "tests completed" in line.lower():
            parts = line.split()
            for i, p in enumerate(parts):
                if p.lower() == "tests" and i >= 1:
                    try:
                        test_count = int(parts[i - 1])
                    except ValueError:
                        pass
                if p.lower() == "failed" and i >= 1:
                    try:
                        failed_count = int(parts[i - 1].rstrip(","))
                    except ValueError:
                        pass
        # BUILD SUCCESSFUL 也可判断
        if "BUILD SUCCESSFUL" in line:
            if rc != 0:
                rc = 0  # Gradle 有时返回非零但实际成功

    duration = time.time() - start
    return TestResult(
        "Android",
        success=(rc == 0),
        duration=duration,
        test_count=test_count,
        failed_count=failed_count,
        output=out,
        error=err,
    )


# ─── iOS 测试 ────────────────────────────────────────────────────────────────

def run_ios_tests() -> TestResult:
    print_header("iOS Unit Tests (Swift Testing)")
    start = time.time()

    if not IOS_WORKSPACE.exists():
        return TestResult(
            "iOS", False, 0,
            error=f"Workspace not found: {IOS_WORKSPACE}\nRun 'cd {IOS_WORKSPACE_DIR} && pod install' first."
        )

    # Check if pods are installed
    pods_dir = IOS_WORKSPACE_DIR / "Pods"
    if not pods_dir.exists():
        print("\n  [0/1] Installing CocoaPods dependencies...")
        rc, out, err = run_command(
            ["pod", "install"],
            cwd=IOS_WORKSPACE_DIR,
            timeout=120,
        )
        if rc != 0:
            return TestResult("iOS", False, time.time() - start, error=err)

    print("\n  Running xcodebuild test...")
    rc, out, err = run_command(
        [
            "xcodebuild", "test",
            "-workspace", str(IOS_WORKSPACE),
            "-scheme", IOS_SCHEME,
            "-destination", IOS_DESTINATION,
            "-quiet",
        ],
        cwd=IOS_WORKSPACE_DIR,
        timeout=300,
    )

    # Parse results
    test_count = None
    failed_count = None
    for line in (out + err).splitlines():
        # e.g. "Test run with 197 tests ... passed"
        if "Test run with" in line and "tests" in line:
            parts = line.split()
            for i, p in enumerate(parts):
                if p == "with" and i + 1 < len(parts):
                    try:
                        test_count = int(parts[i + 1])
                    except ValueError:
                        pass
        # "Executed X tests, with Y failures"
        if "Executed" in line and "tests" in line:
            parts = line.split()
            for i, p in enumerate(parts):
                if p == "Executed" and i + 1 < len(parts):
                    try:
                        test_count = int(parts[i + 1])
                    except ValueError:
                        pass
                if p == "failures" and i >= 1:
                    try:
                        failed_count = int(parts[i - 1])
                    except ValueError:
                        pass
        # ** TEST SUCCEEDED **
        if "TEST SUCCEEDED" in line:
            if rc != 0:
                rc = 0

    duration = time.time() - start
    return TestResult(
        "iOS",
        success=(rc == 0),
        duration=duration,
        test_count=test_count,
        failed_count=failed_count,
        output=out,
        error=err,
    )


# ─── HarmonyOS 测试 ──────────────────────────────────────────────────────────

def run_harmony_tests() -> TestResult:
    print_header("HarmonyOS Unit Tests (Hypium)")
    start = time.time()

    # HarmonyOS 测试分两种：
    # 1. Local unit test (entry/src/test/) — 可用 hvigorw 本地运行
    # 2. Instrumented test (entry/src/ohosTest/) — 需要设备/模拟器

    # 检查 hvigorw 是否存在
    hvigorw = HARMONY_DIR / "hvigorw"
    hvigorw_js = HARMONY_DIR / "hvigorw.js"

    if not hvigorw.exists() and not hvigorw_js.exists():
        # 尝试通过 hvigorw 或 ohpm 环境变量
        if not check_tool("hvigorw"):
            return TestResult(
                "HarmonyOS", False, 0,
                error=(
                    "hvigorw not found. HarmonyOS 测试需要:\n"
                    "  1. DevEco Studio 安装并配置好环境\n"
                    "  2. 或者设置 DEVECO_SDK_HOME 环境变量\n"
                    "  3. 对于 instrumented tests 还需要连接设备/模拟器\n\n"
                    "  手动运行方式:\n"
                    "    cd playground/harmony\n"
                    "    hvigorw --mode module -p module=entry@default -p product=default assembleHap\n"
                    "    hdc install entry-default-signed.hap\n"
                    "    hdc shell aa test -b com.agenui.playground -m entry_test"
                )
            )

    # 尝试运行 local unit tests
    print("\n  Running HarmonyOS local unit tests...")

    # hvigorw 本地单测命令
    cmd = []
    if hvigorw.exists():
        hvigorw.chmod(0o755)
        cmd = [str(hvigorw), "--mode", "module",
               "-p", "module=entry@default",
               "-p", "product=default",
               "test"]
    elif hvigorw_js.exists():
        cmd = ["node", str(hvigorw_js), "--mode", "module",
               "-p", "module=entry@default",
               "-p", "product=default",
               "test"]
    else:
        cmd = ["hvigorw", "--mode", "module",
               "-p", "module=entry@default",
               "-p", "product=default",
               "test"]

    rc, out, err = run_command(cmd, cwd=HARMONY_DIR, timeout=300)

    duration = time.time() - start

    # 如果 hvigorw 不可用，给出说明
    if rc == -1 and "not found" in err.lower():
        return TestResult(
            "HarmonyOS", False, duration,
            error=(
                "HarmonyOS 构建工具不可用。请确保:\n"
                "  - DevEco Studio 已安装\n"
                "  - 或 DEVECO_SDK_HOME / PATH 已配置\n\n"
                "  Instrumented tests 手动运行:\n"
                "    1. 在 DevEco Studio 中打开 playground/harmony\n"
                "    2. 连接设备或启动模拟器\n"
                "    3. 右键 ohosTest → Run"
            )
        )

    return TestResult(
        "HarmonyOS",
        success=(rc == 0),
        duration=duration,
        output=out,
        error=err,
    )


# ─── 主流程 ──────────────────────────────────────────────────────────────────

PLATFORM_RUNNERS = {
    "cpp": run_cpp_tests,
    "c++": run_cpp_tests,
    "android": run_android_tests,
    "ios": run_ios_tests,
    "harmony": run_harmony_tests,
    "harmonyos": run_harmony_tests,
}

ALL_PLATFORMS = ["cpp", "android", "ios", "harmony"]


def main():
    parser = argparse.ArgumentParser(
        description="AGenUI 全平台单元测试运行脚本",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python3 scripts/run_tests.py              # 运行全部平台
  python3 scripts/run_tests.py cpp          # 只运行 C++
  python3 scripts/run_tests.py android ios  # 运行 Android + iOS
  python3 scripts/run_tests.py --verbose    # 显示详细输出
        """,
    )
    parser.add_argument(
        "platforms",
        nargs="*",
        choices=list(PLATFORM_RUNNERS.keys()) + [[]],
        default=[],
        help="要运行的平台 (默认: 全部)",
    )
    parser.add_argument(
        "-v", "--verbose",
        action="store_true",
        help="显示测试详细输出",
    )
    parser.add_argument(
        "--fail-fast",
        action="store_true",
        help="某个平台失败后立即停止",
    )

    args = parser.parse_args()

    # 确定要运行的平台
    platforms = args.platforms if args.platforms else ALL_PLATFORMS

    print(f"\n🚀 AGenUI Unit Test Runner")
    print(f"   Repo: {REPO_ROOT}")
    print(f"   Platforms: {', '.join(platforms)}")

    # 运行测试
    results: list[TestResult] = []
    for platform in platforms:
        runner = PLATFORM_RUNNERS[platform.lower()]
        result = runner()
        results.append(result)
        print_result(result)

        if args.verbose and result.output:
            print("\n  --- stdout ---")
            # 只打印最后 50 行
            lines = result.output.strip().splitlines()
            if len(lines) > 50:
                print(f"  ... ({len(lines) - 50} lines omitted)")
                lines = lines[-50:]
            for line in lines:
                print(f"  {line}")

        if args.verbose and result.error and not result.success:
            print("\n  --- stderr ---")
            lines = result.error.strip().splitlines()
            if len(lines) > 30:
                print(f"  ... ({len(lines) - 30} lines omitted)")
                lines = lines[-30:]
            for line in lines:
                print(f"  {line}")

        if not result.success and result.error and not args.verbose:
            # 非 verbose 模式下也简要输出错误信息
            err_lines = result.error.strip().splitlines()
            if err_lines:
                print(f"  ⚠️  {err_lines[-1]}")

        if args.fail_fast and not result.success:
            print(f"\n  ⛔ --fail-fast: stopping after {platform} failure")
            break

    # 汇总
    print("\n")
    print("=" * 70)
    print("  📊 Test Summary")
    print("=" * 70)

    total_tests = 0
    total_failed = 0
    all_pass = True

    for r in results:
        status = "✅" if r.success else "❌"
        count_info = ""
        if r.test_count is not None:
            total_tests += r.test_count
            count_info = f" ({r.test_count} tests"
            if r.failed_count:
                total_failed += r.failed_count
                count_info += f", {r.failed_count} failed"
            count_info += ")"
        print(f"  {status} {r.platform:<12} {r.duration:>6.1f}s{count_info}")
        if not r.success:
            all_pass = False

    print(f"\n  Total: {total_tests} tests, {total_failed} failed")
    total_time = sum(r.duration for r in results)
    print(f"  Time:  {total_time:.1f}s")

    if all_pass:
        print("\n  🎉 All platforms passed!")
    else:
        failed_platforms = [r.platform for r in results if not r.success]
        print(f"\n  💥 Failed platforms: {', '.join(failed_platforms)}")

    print()
    return 0 if all_pass else 1


if __name__ == "__main__":
    sys.exit(main())
