#!/usr/bin/env python3
"""
AGenUI Stability Test - Crash Log Symbolication

Parses native crash stack traces from crash log files and resolves addresses
to function names, file names, and line numbers using platform-specific tools:
  - Android: llvm-addr2line / ndk-stack (NDK toolchain)
  - iOS: atos (Xcode)
  - HarmonyOS: llvm-addr2line (DevEco/NDK toolchain)

Usage:
    python3 symbolicate.py --input-dir <crashes-dir> --platform android [--symbols <path>]
    python3 symbolicate.py --input-dir <crashes-dir> --platform ios [--dsym <path>]
    python3 symbolicate.py --input-dir <crashes-dir> --platform harmony [--symbols <path>]

The script produces .symbolicated.txt files alongside each crash/freeze log file.
"""

import argparse
import glob
import os
import re
import subprocess
import sys
from pathlib import Path


# ============================================================================
# Address parsing patterns
# ============================================================================

# Android: #00 pc 0000000000160730  /path/to/lib.so (optional_func) (BuildId: xxx)
ANDROID_FRAME_RE = re.compile(
    r'#(\d+)\s+pc\s+([0-9a-fA-F]+)\s+(/\S+\.so)\b'
)

# iOS .ips / crash report: 4  AppName  0x00000001a2345678  0x1a2000000 + 3456
IOS_FRAME_RE = re.compile(
    r'(\d+)\s+(\S+)\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s*\+\s*(\d+)'
)

# HarmonyOS faultlog: #00 pc 0000000000160730  /data/.../lib.so
HARMONY_FRAME_RE = re.compile(
    r'#(\d+)\s+pc\s+([0-9a-fA-F]+)\s+(/\S+\.so)\b'
)


# ============================================================================
# Tool discovery
# ============================================================================

def find_llvm_addr2line():
    """Find llvm-addr2line from NDK, DevEco Studio, or system PATH."""
    # Check PATH first
    result = subprocess.run(['which', 'llvm-addr2line'], capture_output=True, text=True)
    if result.returncode == 0:
        return result.stdout.strip()

    # DevEco Studio (HarmonyOS)
    deveco_paths = [
        '/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin/llvm-addr2line',
        '/Applications/DevEco-Studio.app/Contents/sdk/default/hms/native/BiSheng/bin/llvm-addr2line',
    ]
    for dp in deveco_paths:
        if os.path.isfile(dp):
            return dp

    # Search Android NDK
    ndk_paths = [
        os.environ.get('ANDROID_NDK_HOME', ''),
        os.environ.get('ANDROID_NDK', ''),
    ]
    # Common SDK locations
    home = os.path.expanduser('~')
    ndk_paths.extend(glob.glob(f'{home}/Library/Android/sdk/ndk/*/'))
    ndk_paths.extend(glob.glob('/usr/local/android-ndk*/'))

    for ndk_path in ndk_paths:
        if not ndk_path:
            continue
        candidates = glob.glob(
            os.path.join(ndk_path, 'toolchains/llvm/prebuilt/*/bin/llvm-addr2line')
        )
        if candidates:
            # Sort to pick latest
            candidates.sort(reverse=True)
            return candidates[0]

    return None


def find_atos():
    """Find atos tool (macOS/Xcode)."""
    result = subprocess.run(['which', 'atos'], capture_output=True, text=True)
    if result.returncode == 0:
        return result.stdout.strip()
    # Xcode default
    xcode_path = '/usr/bin/atos'
    if os.path.isfile(xcode_path):
        return xcode_path
    return None


def find_symbol_file(platform, repo_root, lib_name='libamap_AGenUI.so'):
    """Auto-discover unstripped symbol files."""
    candidates = []

    if platform == 'android':
        # Debug build contains unstripped .so with debug_info
        patterns = [
            f'platforms/android/build/intermediates/merged_native_libs/debug/*/out/lib/arm64-v8a/{lib_name}',
            f'playground/android/app/build/intermediates/merged_native_libs/debug/*/out/lib/arm64-v8a/{lib_name}',
            f'platforms/android/build/intermediates/cxx/Debug/*/obj/arm64-v8a/{lib_name}',
        ]
        for pattern in patterns:
            matches = glob.glob(os.path.join(repo_root, pattern))
            candidates.extend(matches)

    elif platform == 'harmony':
        patterns = [
            'playground/harmony/entry/build/default/intermediates/libs/default/arm64-v8a/liba2ui-capi.so',
            'playground/harmony/entry/build/default/intermediates/libs/ohosTest/arm64-v8a/liba2ui-capi.so',
            'platforms/harmony/agenui/build/default/intermediates/libs/default/arm64-v8a/liba2ui-capi.so',
            '**/symbols/arm64-v8a/liba2ui-capi.so',
        ]
        for pattern in patterns:
            matches = glob.glob(os.path.join(repo_root, pattern), recursive=True)
            candidates.extend(matches)

    elif platform == 'ios':
        # Look for dSYM
        patterns = [
            'playground/ios/Playground/build/tmp/iphoneos/sym/*.dSYM',
            'playground/ios/build/**/*.dSYM',
            '**/AGenUI.framework.dSYM',
        ]
        for pattern in patterns:
            matches = glob.glob(os.path.join(repo_root, pattern), recursive=True)
            candidates.extend(matches)

    # Filter: only return files that exist and are non-empty
    return [c for c in candidates if os.path.exists(c) and os.path.getsize(c) > 0]


# ============================================================================
# Symbolication engines
# ============================================================================

def symbolicate_android(crash_content, addr2line_path, symbol_files):
    """
    Symbolicate Android native crash frames using llvm-addr2line.
    Returns the crash content with addresses resolved to function/file/line.
    """
    # Group addresses by library
    lib_frames = {}  # lib_path -> [(frame_idx, addr, line_text)]
    lines = crash_content.split('\n')

    for i, line in enumerate(lines):
        m = ANDROID_FRAME_RE.search(line)
        if m:
            frame_num, addr, lib_path = m.group(1), m.group(2), m.group(3)
            lib_name = os.path.basename(lib_path)
            if lib_name not in lib_frames:
                lib_frames[lib_name] = []
            lib_frames[lib_name].append((i, addr, line))

    if not lib_frames:
        return None  # No native frames to symbolicate

    # Resolve symbols for each library
    resolved = {}  # line_index -> resolved_text

    for lib_name, frames in lib_frames.items():
        # Find matching symbol file
        sym_file = None
        for sf in symbol_files:
            if os.path.basename(sf) == lib_name:
                sym_file = sf
                break

        if not sym_file:
            continue

        # Batch addr2line call (much faster than per-frame)
        addrs = [f'0x{frame[1]}' for frame in frames]
        try:
            result = subprocess.run(
                [addr2line_path, '-C', '-f', '-e', sym_file] + addrs,
                capture_output=True, text=True, timeout=30
            )
            if result.returncode != 0:
                continue

            # Parse output: alternating function_name and file:line
            output_lines = result.stdout.strip().split('\n')
            for j, (line_idx, addr, original_line) in enumerate(frames):
                func_idx = j * 2
                loc_idx = func_idx + 1
                if loc_idx < len(output_lines):
                    func_name = output_lines[func_idx].strip()
                    location = output_lines[loc_idx].strip()
                    if func_name != '??' or location != '??:0':
                        # Format: original_line  → function (file:line)
                        resolution = f'{func_name} at {location}'
                        resolved[line_idx] = resolution

        except (subprocess.TimeoutExpired, OSError):
            continue

    if not resolved:
        return None

    # Build symbolicated output
    output_lines = lines[:]
    for line_idx, resolution in resolved.items():
        output_lines[line_idx] = f'{lines[line_idx]}  → {resolution}'

    return '\n'.join(output_lines)


def symbolicate_ios(crash_content, atos_path, dsym_paths):
    """
    Symbolicate iOS crash frames using atos.
    """
    lines = crash_content.split('\n')
    resolved = {}

    # Parse all frames
    frames_by_binary = {}  # binary_name -> [(line_idx, load_addr, addr)]
    for i, line in enumerate(lines):
        m = IOS_FRAME_RE.search(line)
        if m:
            frame_num, binary, addr, load_addr, offset = m.groups()
            if binary not in frames_by_binary:
                frames_by_binary[binary] = []
            frames_by_binary[binary].append((i, load_addr, addr))

    if not frames_by_binary:
        return None

    for binary, frames in frames_by_binary.items():
        # Find dSYM for this binary
        dsym = None
        for d in dsym_paths:
            if binary in d or 'AGenUI' in d:
                dsym = d
                break
        if not dsym:
            continue

        # Use atos for each unique load_addr group
        load_groups = {}
        for line_idx, load_addr, addr in frames:
            if load_addr not in load_groups:
                load_groups[load_addr] = []
            load_groups[load_addr].append((line_idx, addr))

        for load_addr, group_frames in load_groups.items():
            addrs = [f[1] for f in group_frames]
            try:
                cmd = [atos_path, '-o', dsym, '-l', load_addr] + addrs
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
                if result.returncode != 0:
                    continue

                symbols = result.stdout.strip().split('\n')
                for j, (line_idx, _) in enumerate(group_frames):
                    if j < len(symbols) and symbols[j] and '0x' not in symbols[j]:
                        resolved[line_idx] = symbols[j].strip()

            except (subprocess.TimeoutExpired, OSError):
                continue

    if not resolved:
        return None

    output_lines = lines[:]
    for line_idx, resolution in resolved.items():
        output_lines[line_idx] = f'{lines[line_idx]}  → {resolution}'

    return '\n'.join(output_lines)


def symbolicate_harmony(crash_content, addr2line_path, symbol_files):
    """
    Symbolicate HarmonyOS native crash frames (same format as Android).
    """
    # HarmonyOS uses the same tombstone-style format as Android
    return symbolicate_android(crash_content, addr2line_path, symbol_files)


# ============================================================================
# Main processing
# ============================================================================

def process_crash_files(input_dir, platform, symbol_paths=None, dsym_paths=None):
    """Process all crash/freeze files in the directory."""
    crash_dir = os.path.join(input_dir, 'crashes')
    if not os.path.isdir(crash_dir):
        print(f'[Symbolicate] No crashes directory found: {crash_dir}')
        return 0

    # Find tool
    if platform in ('android', 'harmony'):
        addr2line = find_llvm_addr2line()
        if not addr2line:
            print('[Symbolicate] ERROR: llvm-addr2line not found. Install Android NDK or add to PATH.')
            return 1
        print(f'[Symbolicate] Using: {addr2line}')
    elif platform == 'ios':
        atos = find_atos()
        if not atos:
            print('[Symbolicate] ERROR: atos not found. Install Xcode Command Line Tools.')
            return 1
        print(f'[Symbolicate] Using: {atos}')

    # Find symbol files
    # Detect repo root: look for reports/stability/runs in the path and go up
    repo_root = None
    input_abs = os.path.abspath(input_dir)
    marker = os.path.sep + 'reports' + os.path.sep + 'stability' + os.path.sep + 'runs'
    idx = input_abs.find(marker)
    if idx > 0:
        repo_root = input_abs[:idx]
    else:
        # Fallback: try git root
        try:
            result = subprocess.run(
                ['git', 'rev-parse', '--show-toplevel'],
                capture_output=True, text=True, cwd=input_dir
            )
            if result.returncode == 0:
                repo_root = result.stdout.strip()
        except OSError:
            pass
    if not repo_root:
        # Last fallback: script directory
        repo_root = str(Path(__file__).parents[2])

    print(f'[Symbolicate] Repo root: {repo_root}')

    if platform in ('android', 'harmony'):
        if symbol_paths:
            sym_files = symbol_paths
        else:
            sym_files = find_symbol_file(platform, repo_root)
        if not sym_files:
            print(f'[Symbolicate] WARNING: No unstripped symbol files found for {platform}')
            print(f'[Symbolicate] Searched in: {repo_root}')
            print(f'[Symbolicate] Build debug first: ./gradlew :app:assembleDebug')
            return 1
        print(f'[Symbolicate] Symbol files: {sym_files}')
    elif platform == 'ios':
        if dsym_paths:
            sym_files = dsym_paths
        else:
            sym_files = find_symbol_file(platform, repo_root)
        if not sym_files:
            print('[Symbolicate] WARNING: No dSYM files found for iOS')
            return 1
        print(f'[Symbolicate] dSYM files: {sym_files}')

    # Process each crash/freeze file
    # Include Android/iOS crash_*.txt + freeze_*.txt and HarmonyOS cppcrash-*.log + jscrash-*.log
    crash_files = sorted(set(
        glob.glob(os.path.join(crash_dir, 'crash_*.txt')) +
        glob.glob(os.path.join(crash_dir, 'freeze_*.txt')) +
        glob.glob(os.path.join(crash_dir, 'cppcrash-*.log')) +
        glob.glob(os.path.join(crash_dir, 'jscrash-*.log'))
    ))
    # Exclude already-symbolicated files
    crash_files = [f for f in crash_files if '.symbolicated.' not in f]

    if not crash_files:
        print('[Symbolicate] No crash/freeze files to process')
        return 0

    symbolicated_count = 0
    for crash_file in crash_files:
        basename = os.path.basename(crash_file)
        sym_output = os.path.join(
            crash_dir,
            basename.replace('.log', '.symbolicated.log') if basename.endswith('.log')
            else basename.replace('.txt', '.symbolicated.txt')
        )

        # Skip if already symbolicated
        if os.path.isfile(sym_output):
            print(f'[Symbolicate] Skip (exists): {basename}')
            symbolicated_count += 1
            continue

        print(f'[Symbolicate] Processing: {basename}...')

        try:
            with open(crash_file, 'r', encoding='utf-8', errors='replace') as f:
                content = f.read()
        except OSError as e:
            print(f'[Symbolicate]   ERROR reading: {e}')
            continue

        # Symbolicate
        result = None
        if platform == 'android':
            result = symbolicate_android(content, addr2line, sym_files)
        elif platform == 'ios':
            result = symbolicate_ios(content, atos, sym_files)
        elif platform == 'harmony':
            result = symbolicate_harmony(content, addr2line, sym_files)

        if result:
            with open(sym_output, 'w', encoding='utf-8') as f:
                f.write(result)
            symbolicated_count += 1
            # Count resolved frames
            arrow_count = result.count(' → ')
            print(f'[Symbolicate]   OK: {arrow_count} frames resolved -> {os.path.basename(sym_output)}')
        else:
            print(f'[Symbolicate]   No native frames found or resolution failed')

    print(f'[Symbolicate] Done: {symbolicated_count}/{len(crash_files)} files symbolicated')
    return 0


def main():
    parser = argparse.ArgumentParser(description='Symbolicate crash logs from stability test')
    parser.add_argument('--input-dir', required=True,
                        help='Path to platform output directory (contains crashes/ subdirectory)')
    parser.add_argument('--platform', required=True, choices=['android', 'ios', 'harmony'],
                        help='Target platform')
    parser.add_argument('--symbols', nargs='*',
                        help='Paths to unstripped symbol files (.so / .dSYM)')
    args = parser.parse_args()

    if not os.path.isdir(args.input_dir):
        print(f'[Symbolicate] Error: directory not found: {args.input_dir}')
        return 1

    symbol_paths = args.symbols if args.symbols else None
    dsym_paths = args.symbols if args.symbols and args.platform == 'ios' else None

    return process_crash_files(
        args.input_dir,
        args.platform,
        symbol_paths=symbol_paths if args.platform != 'ios' else None,
        dsym_paths=dsym_paths
    )


if __name__ == '__main__':
    sys.exit(main())
