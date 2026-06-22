#!/usr/bin/env python3
"""
AGenUI Stability Test HTML Report Generator

Reads stability test output files (metadata.json, monitor_summary.json,
collection_summary.json, stability_log.jsonl, crashes/) and generates
a comprehensive HTML report.

Usage:
    python3 generate_report.py --input-dir <path-to-platform-output-dir>
"""

import argparse
import json
import os
import glob
import html
import re
from pathlib import Path


def load_json(filepath):
    """Load a JSON file, return empty dict on failure."""
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read().strip()
            return json.loads(content)
    except json.JSONDecodeError:
        # Try to fix common malformation: duplicate values from shell grep -c
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                content = f.read()
            # Remove stray numbers on their own line before a comma
            content = re.sub(r':\s*(\d+)\s*\n\d+,', r': \1,', content)
            return json.loads(content)
        except (json.JSONDecodeError, OSError):
            return {}
    except OSError:
        return {}


def load_jsonl_rounds(filepath):
    """Load round entries from JSONL file."""
    rounds = []
    if not os.path.isfile(filepath):
        return rounds
    with open(filepath, 'r', encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                entry = json.loads(line)
                if 'round' in entry:
                    rounds.append(entry)
            except json.JSONDecodeError:
                continue
    return rounds


def load_crash_files(crash_dir):
    """Load crash file summaries. Prefer .symbolicated.txt if available.
    Extracts the most relevant crash section (header + signal + backtrace).
    Supports both Android crash_*.txt and HarmonyOS cppcrash-*/jscrash-*.log formats."""
    crashes = []
    if not os.path.isdir(crash_dir):
        return crashes

    # Collect crash files: Android format (crash_*.txt) + HarmonyOS format (cppcrash-*.log, jscrash-*.log)
    crash_files = set()
    for pattern in ['crash_*.txt', 'cppcrash-*.log', 'jscrash-*.log']:
        crash_files.update(glob.glob(os.path.join(crash_dir, pattern)))

    for filepath in sorted(crash_files):
        # Skip .symbolicated files (they are loaded as the preferred version of the original)
        if '.symbolicated.' in filepath:
            continue
        try:
            # Prefer symbolicated version
            ext = '.log' if filepath.endswith('.log') else '.txt'
            sym_path = filepath.replace(ext, '.symbolicated' + ext)
            is_symbolicated = os.path.isfile(sym_path)
            read_path = sym_path if is_symbolicated else filepath

            with open(read_path, 'r', encoding='utf-8', errors='replace') as f:
                content = f.read()
            all_lines = content.split('\n')
            # Extract crash-relevant summary: header + crash signal + backtrace
            summary_lines = _extract_crash_summary(all_lines)
            crashes.append({
                'filename': os.path.basename(filepath),
                'summary': '\n'.join(summary_lines),
                'full_lines': len(all_lines),
                'symbolicated': is_symbolicated,
                'raw_filename': os.path.basename(filepath),
                'sym_filename': os.path.basename(sym_path) if is_symbolicated else None,
            })
        except OSError:
            continue
    return crashes


def _extract_crash_summary(lines, max_lines=80):
    """Extract the most relevant crash section from a crash log.
    Supports three formats:
    1. Java FATAL EXCEPTION (Android) — highest priority
    2. Android native crash (tombstone/dropbox)
    3. HarmonyOS faultlog (cppcrash/jscrash from HiviewDFX)"""
    # Always include header (first 5 lines: crash info, device time, scenario)
    header = lines[:5]

    # --- Phase 1: Look for Java FATAL EXCEPTION (highest priority) ---
    java_fatal_idx = -1
    for i, line in enumerate(lines):
        if 'FATAL EXCEPTION' in line and 'AndroidRuntime' in line:
            java_fatal_idx = i
            break

    if java_fatal_idx >= 0:
        result = list(header)
        if java_fatal_idx > 5:
            result.append('')
            result.append('... (logcat trimmed) ...')
            result.append('')
        # Extract all consecutive AndroidRuntime lines (the full Java stack trace)
        java_end = java_fatal_idx
        while java_end < len(lines):
            if 'AndroidRuntime' in lines[java_end] or (
                java_end == java_fatal_idx  # always include the first line
            ):
                java_end += 1
            else:
                break
        result.extend(lines[java_fatal_idx:java_end])

        # Also note if there's a native crash in the same file (for context)
        has_native = any(
            'signal 11' in l or 'signal 6' in l or 'SIGSEGV' in l or 'SIGABRT' in l
            for l in lines[java_end:]
        )
        if has_native:
            result.append('')
            result.append('--- (file also contains a native crash record, likely from a different process/run) ---')

        if len(result) > max_lines:
            result = result[:max_lines]
        return result

    # --- Phase 2: HarmonyOS faultlog detection (HiviewDFX format) ---
    # HarmonyOS cppcrash format: "Reason:Signal:SIGSEGV..." and "Fault thread info:" with "#XX pc" frames
    harmony_reason_idx = -1
    harmony_fault_thread_idx = -1
    for i, line in enumerate(lines):
        if line.startswith('Reason:') and harmony_reason_idx == -1:
            harmony_reason_idx = i
        if 'Fault thread info:' in line and harmony_fault_thread_idx == -1:
            harmony_fault_thread_idx = i

    if harmony_reason_idx >= 0 or harmony_fault_thread_idx >= 0:
        # Include header lines up to and including Reason line (but stop before Fault thread info)
        if harmony_fault_thread_idx >= 0 and harmony_reason_idx >= 0:
            end_header = min(harmony_fault_thread_idx, harmony_reason_idx + 1)
        elif harmony_reason_idx >= 0:
            end_header = harmony_reason_idx + 1
        else:
            end_header = min(len(lines), 21)
        result = lines[:end_header]

        # Include fault thread info and stack frames
        if harmony_fault_thread_idx >= 0:
            if harmony_fault_thread_idx > end_header:
                result.append('')
                result.append('... (trimmed) ...')
                result.append('')
            # Include "Fault thread info:" header + thread name + all #XX pc frames
            bt_start = harmony_fault_thread_idx
            bt_end = bt_start + 1
            while bt_end < len(lines) and bt_end < bt_start + 50:
                stripped = lines[bt_end].strip()
                if stripped.startswith('#') or stripped.startswith('Tid:'):
                    bt_end += 1
                elif bt_end == bt_start + 1:
                    bt_end += 1
                else:
                    break
            result.extend(lines[bt_start:bt_end])

        if len(result) > max_lines:
            result = result[:max_lines]
        return result

    # --- Phase 3: Fall back to Android native crash detection ---
    fatal_idx = -1
    backtrace_start = -1
    for i, line in enumerate(lines):
        if 'Fatal signal' in line or 'SIGSEGV' in line or 'SIGABRT' in line:
            if fatal_idx == -1:
                fatal_idx = i
        if 'backtrace:' in line.lower():
            backtrace_start = i
            break

    # Build summary
    result = list(header)

    if fatal_idx > 4:
        result.append('')
        result.append('... (logcat trimmed) ...')
        result.append('')
        # Include 2 lines before Fatal signal for context
        start = max(fatal_idx - 2, 5)
        end = min(fatal_idx + 3, len(lines))
        result.extend(lines[start:end])

    if backtrace_start > 0:
        if backtrace_start > fatal_idx + 3:
            result.append('')
            result.append('... (registers trimmed) ...')
            result.append('')
        # Include backtrace header and all frame lines (up to 30 frames)
        bt_end = backtrace_start + 1
        while bt_end < len(lines) and bt_end < backtrace_start + 35:
            if lines[bt_end].strip() and ('#' in lines[bt_end] or 'pc ' in lines[bt_end]):
                bt_end += 1
            elif bt_end == backtrace_start + 1:
                # First line after backtrace: might be a frame
                bt_end += 1
            else:
                break
        result.extend(lines[backtrace_start:bt_end])

    # If no crash-specific content found, fall back to first N lines
    if fatal_idx == -1 and backtrace_start == -1:
        return lines[:max_lines]

    # Cap total length
    if len(result) > max_lines:
        result = result[:max_lines]

    return result


def load_freeze_files(crash_dir):
    """Load freeze file summaries. Prefer .symbolicated.txt if available."""
    freezes = []
    if not os.path.isdir(crash_dir):
        return freezes
    for filepath in sorted(glob.glob(os.path.join(crash_dir, 'freeze_*.txt'))):
        if filepath.endswith('.symbolicated.txt'):
            continue
        try:
            sym_path = filepath.replace('.txt', '.symbolicated.txt')
            is_symbolicated = os.path.isfile(sym_path)
            read_path = sym_path if is_symbolicated else filepath

            with open(read_path, 'r', encoding='utf-8', errors='replace') as f:
                content = f.read()
            lines = content.split('\n')
            summary_lines = lines[:80]
            freezes.append({
                'filename': os.path.basename(filepath),
                'summary': '\n'.join(summary_lines),
                'full_lines': len(lines),
                'symbolicated': is_symbolicated,
                'raw_filename': os.path.basename(filepath),
                'sym_filename': os.path.basename(sym_path) if is_symbolicated else None,
            })
        except OSError:
            continue
    return freezes


def percentile(sorted_list, p):
    """Calculate percentile from a sorted list."""
    if not sorted_list:
        return 0
    idx = int(len(sorted_list) * p / 100)
    idx = min(idx, len(sorted_list) - 1)
    return sorted_list[idx]


def generate_svg_chart(mem_points, width=600, height=120):
    """Generate an inline SVG memory trend chart."""
    if not mem_points or len(mem_points) < 2:
        return '<p style="color:#aaa;font-size:13px;">No memory data available</p>'

    # Downsample to max 100 points
    if len(mem_points) > 100:
        step = len(mem_points) // 100
        mem_points = mem_points[::step]

    rounds = [p[0] for p in mem_points]
    mems = [p[1] for p in mem_points]

    min_mem = min(mems) * 0.9
    max_mem = max(mems) * 1.1
    if max_mem == min_mem:
        max_mem = min_mem + 1

    padding_left = 45
    padding_right = 10
    padding_top = 10
    padding_bottom = 25
    chart_w = width - padding_left - padding_right
    chart_h = height - padding_top - padding_bottom

    def x_pos(i):
        return padding_left + (i / (len(mem_points) - 1)) * chart_w

    def y_pos(val):
        return padding_top + (1 - (val - min_mem) / (max_mem - min_mem)) * chart_h

    # Build polyline points
    points = ' '.join(f'{x_pos(i):.1f},{y_pos(m):.1f}' for i, m in enumerate(mems))
    # Build area fill
    area_points = f'{x_pos(0):.1f},{padding_top + chart_h:.1f} '
    area_points += points
    area_points += f' {x_pos(len(mems)-1):.1f},{padding_top + chart_h:.1f}'

    # Y-axis labels
    y_labels = ''
    for i in range(5):
        val = min_mem + (max_mem - min_mem) * i / 4
        y = y_pos(val)
        y_labels += f'<text x="{padding_left - 5}" y="{y:.1f}" text-anchor="end" font-size="10" fill="#aaa">{val:.0f}</text>'

    # X-axis labels
    x_labels = ''
    for i in [0, len(mem_points) // 2, len(mem_points) - 1]:
        x = x_pos(i)
        x_labels += f'<text x="{x:.1f}" y="{height - 3}" text-anchor="middle" font-size="10" fill="#aaa">R{rounds[i]}</text>'

    svg = f'''<svg width="{width}" height="{height}" xmlns="http://www.w3.org/2000/svg" style="background:#fff;border-radius:8px;">
  <defs>
    <linearGradient id="memGrad" x1="0%" y1="0%" x2="0%" y2="100%">
      <stop offset="0%" style="stop-color:#2196f3;stop-opacity:0.3"/>
      <stop offset="100%" style="stop-color:#2196f3;stop-opacity:0.02"/>
    </linearGradient>
  </defs>
  <!-- Grid lines -->
  <line x1="{padding_left}" y1="{padding_top}" x2="{padding_left}" y2="{padding_top + chart_h}" stroke="#eee" stroke-width="1"/>
  <line x1="{padding_left}" y1="{padding_top + chart_h}" x2="{padding_left + chart_w}" y2="{padding_top + chart_h}" stroke="#eee" stroke-width="1"/>
  <!-- Area fill -->
  <polygon points="{area_points}" fill="url(#memGrad)"/>
  <!-- Line -->
  <polyline points="{points}" fill="none" stroke="#2196f3" stroke-width="2" stroke-linejoin="round"/>
  <!-- Labels -->
  {y_labels}
  {x_labels}
  <text x="{padding_left - 5}" y="{padding_top - 2}" text-anchor="end" font-size="9" fill="#aaa">MB</text>
</svg>'''
    return svg


def generate_report(input_dir):
    """Generate HTML report from stability test output."""
    input_dir = os.path.abspath(input_dir)
    parent_dir = os.path.dirname(input_dir)

    # Load data sources
    metadata = load_json(os.path.join(parent_dir, 'metadata.json'))
    monitor = load_json(os.path.join(input_dir, 'monitor_summary.json'))
    collection = load_json(os.path.join(input_dir, 'collection_summary.json'))
    rounds = load_jsonl_rounds(os.path.join(input_dir, 'stability_log.jsonl'))
    crashes = load_crash_files(os.path.join(input_dir, 'crashes'))
    freezes = load_freeze_files(os.path.join(input_dir, 'crashes'))

    # Extract key values
    duration_min = metadata.get('duration_minutes', monitor.get('duration_minutes', 0))
    crash_count = monitor.get('crash_count', len(crashes))
    freeze_count = monitor.get('freeze_count', len(freezes))
    crash_threshold = monitor.get('crash_threshold', 5)
    crash_scenarios = monitor.get('crash_scenarios', [])
    freeze_scenarios = monitor.get('freeze_scenarios', [])
    total_rounds = collection.get('total_rounds', len(rounds))
    ok_count = collection.get('ok_count', total_rounds)
    error_count = collection.get('error_count', 0)
    def _safe_float(val, default=0):
        try:
            return float(val) if val and val != 'N/A' else default
        except (ValueError, TypeError):
            return default
    mem_start = _safe_float(collection.get('memory_start_mb', 0))
    mem_end = _safe_float(collection.get('memory_end_mb', 0))

    # Fallback: extract memory from JSONL if collection_summary failed
    if mem_start == 0 and rounds:
        mem_entries = [r.get('memory_total_mb') or r.get('memory_mb') for r in rounds if 'memory_total_mb' in r or 'memory_mb' in r]
        if mem_entries:
            mem_start = mem_entries[0]
            mem_end = mem_entries[-1]

    # Git info
    git_branch = metadata.get('git_branch', '-')
    git_commit = metadata.get('git_commit', '-')
    start_time = metadata.get('start_time', '-')
    end_time = monitor.get('end_time', collection.get('collected_at', '-'))
    scenario = metadata.get('scenario', '-')
    platform = metadata.get('platform', 'android')

    # Verdict
    passed = crash_count == 0 and freeze_count == 0
    verdict_text = 'PASS' if passed else 'FAIL'
    verdict_color = '#27ae60' if passed else '#e74c3c'
    verdict_bg = '#e8f8f0' if passed else '#fde8e8'

    # Performance metrics from JSONL
    durations = sorted([r.get('duration_ms', 0) for r in rounds if 'duration_ms' in r])
    avg_duration = sum(durations) / len(durations) if durations else 0
    p50 = percentile(durations, 50)
    p95 = percentile(durations, 95)
    p99 = percentile(durations, 99)
    max_duration = max(durations) if durations else 0

    throughput = total_rounds / duration_min if duration_min > 0 else 0
    success_rate = (ok_count / total_rounds * 100) if total_rounds > 0 else 0

    # Memory analysis
    mem_growth = mem_end - mem_start
    duration_hours = duration_min / 60 if duration_min > 0 else 1
    growth_rate = mem_growth / duration_hours if duration_hours > 0 else 0
    mem_points = [(r['round'], r.get('memory_total_mb') or r.get('memory_mb')) for r in rounds if 'memory_total_mb' in r or 'memory_mb' in r]
    peak_mem = max([p[1] for p in mem_points]) if mem_points else mem_end

    # Memory leak risk
    if growth_rate > 20:
        leak_risk = 'HIGH'
        leak_color = '#e74c3c'
    elif growth_rate > 5:
        leak_risk = 'MEDIUM'
        leak_color = '#f39c12'
    else:
        leak_risk = 'LOW'
        leak_color = '#27ae60'

    # MTBF
    if crash_count > 0:
        mtbf_min = duration_min / crash_count
        mtbf_text = f'{mtbf_min:.1f} min'
    else:
        mtbf_text = f'{duration_min} min (no crash)'

    # Scenario breakdown
    scenario_stats = {}
    for r in rounds:
        s = r.get('scenario', 'UNKNOWN')
        if s not in scenario_stats:
            scenario_stats[s] = {'rounds': 0, 'ok': 0, 'errors': 0, 'durations': [], 'crashes': 0}
        scenario_stats[s]['rounds'] += 1
        if r.get('status') == 'ok':
            scenario_stats[s]['ok'] += 1
        else:
            scenario_stats[s]['errors'] += 1
        if 'duration_ms' in r:
            scenario_stats[s]['durations'].append(r['duration_ms'])

    # Count crashes per scenario
    for cs in crash_scenarios:
        if cs in scenario_stats:
            scenario_stats[cs]['crashes'] += 1
        else:
            scenario_stats[cs] = {'rounds': 0, 'ok': 0, 'errors': 0, 'durations': [], 'crashes': 1}

    # Sort by error rate descending
    scenario_rows = sorted(scenario_stats.items(),
                           key=lambda x: (x[1]['crashes'], x[1]['errors']), reverse=True)

    # Build scenario table HTML
    scenario_table_rows = ''
    for name, stats in scenario_rows:
        s_total = stats['rounds']
        s_rate = (stats['ok'] / s_total * 100) if s_total > 0 else 0
        s_avg = (sum(stats['durations']) / len(stats['durations'])) if stats['durations'] else 0
        s_crashes = stats['crashes']
        rate_color = '#27ae60' if s_rate >= 99 else ('#f39c12' if s_rate >= 95 else '#e74c3c')
        crash_badge = f'<span style="color:#e74c3c;font-weight:600">{s_crashes}</span>' if s_crashes > 0 else '0'
        scenario_table_rows += f'''<tr>
  <td style="font-weight:600">{html.escape(name)}</td>
  <td>{s_total}</td>
  <td style="color:{rate_color}">{s_rate:.1f}%</td>
  <td>{s_avg:.0f} ms</td>
  <td>{crash_badge}</td>
</tr>'''

    # Build crash analysis HTML
    crash_html = ''
    if crashes:
        crash_timeline = '<div style="margin-bottom:16px;">'
        for i, cs in enumerate(crash_scenarios):
            crash_timeline += f'<span style="display:inline-block;margin:2px 6px 2px 0;padding:3px 10px;border-radius:10px;background:#fde8e8;color:#e74c3c;font-size:12px;font-weight:500">#{i+1} {html.escape(cs)}</span>'
        crash_timeline += '</div>'

        crash_details = ''
        for crash in crashes:
            # Highlight signal lines in crash content
            summary_escaped = html.escape(crash['summary'])
            # Highlight crash signals (Android + HarmonyOS)
            for keyword in ['signal 11', 'SIGSEGV', 'SEGV_MAPERR', 'signal 6', 'SIGABRT',
                            'Fatal signal', 'backtrace', 'Fault thread info',
                            'NULL pointer dereference', 'Reason:']:
                summary_escaped = summary_escaped.replace(
                    keyword,
                    f'<span style="background:#fde8e8;color:#e74c3c;font-weight:700;padding:0 3px;border-radius:3px">{keyword}</span>'
                )
            # Highlight symbolicated resolution arrows (→ function at file:line)
            summary_escaped = re.sub(
                r'→ (.+?) at (.+?:\d+)',
                r'→ <span style="color:#1a73e8;font-weight:700">\1</span> at <span style="color:#27ae60;font-style:italic">\2</span>',
                summary_escaped
            )
            # Also highlight standalone → symbol_name patterns
            summary_escaped = re.sub(
                r'→ ([^\n<]+)',
                r'→ <span style="color:#1a73e8;font-weight:700">\1</span>',
                summary_escaped
            )
            # Symbolication badge
            sym_badge = ''
            if crash.get('symbolicated'):
                sym_badge = '<span style="margin-left:8px;padding:2px 6px;border-radius:4px;background:#e8f5e9;color:#2e7d32;font-size:10px;font-weight:600;">SYMBOLICATED</span>'
            else:
                sym_badge = '<span style="margin-left:8px;padding:2px 6px;border-radius:4px;background:#fff3e0;color:#e65100;font-size:10px;font-weight:600;">RAW</span>'

            # Build links: raw log link + symbolicated link if available
            links_html = f'<a href="crashes/{html.escape(crash["raw_filename"])}" target="_blank" style="margin-left:8px;font-size:11px;color:#1a73e8;text-decoration:none;" title="Open raw log">&#128279; raw</a>'
            if crash.get('sym_filename'):
                links_html += f' <a href="crashes/{html.escape(crash["sym_filename"])}" target="_blank" style="margin-left:4px;font-size:11px;color:#2e7d32;text-decoration:none;" title="Open symbolicated log">&#128279; symbolicated</a>'

            crash_details += f'''<details style="margin-bottom:8px;">
  <summary style="cursor:pointer;padding:8px 12px;background:#fafafa;border-radius:8px;font-size:13px;font-weight:500;">
    <span class="arrow">&#9654;</span> {html.escape(crash['filename'])} ({crash['full_lines']} lines)
    {sym_badge}
    {links_html}
  </summary>
  <pre style="margin:8px 0 0 0;padding:12px;background:#1a1a2e;color:#e0e0e0;border-radius:8px;font-size:11px;line-height:1.5;overflow-x:auto;max-height:400px;overflow-y:auto;">{summary_escaped}</pre>
</details>'''

        crash_html = f'''<div class="section">
  <h2>Crash Analysis</h2>
  <div style="margin-bottom:12px;font-size:13px;color:#666;">
    Crash distribution by scenario:
  </div>
  {crash_timeline}
  {crash_details}
</div>'''
    else:
        crash_html = '''<div class="section">
  <h2>Crash Analysis</h2>
  <div style="padding:24px;text-align:center;color:#27ae60;font-size:14px;font-weight:500;">
    No crashes detected during this test run.
  </div>
</div>'''

    # Build freeze analysis HTML
    freeze_html = ''
    if freezes:
        freeze_timeline = '<div style="margin-bottom:16px;">'
        for i, fs in enumerate(freeze_scenarios):
            freeze_timeline += f'<span style="display:inline-block;margin:2px 6px 2px 0;padding:3px 10px;border-radius:10px;background:#fff3e0;color:#e65100;font-size:12px;font-weight:500">#{i+1} {html.escape(fs)}</span>'
        freeze_timeline += '</div>'

        freeze_details = ''
        for freeze in freezes:
            summary_escaped = html.escape(freeze['summary'])
            # Highlight ANR-related keywords
            for keyword in ['ANR', 'deadlock', 'blocked', 'waiting on', 'MONITOR', 'main']:
                summary_escaped = summary_escaped.replace(
                    keyword,
                    f'<span style="background:#fff3e0;color:#e65100;font-weight:700;padding:0 3px;border-radius:3px">{keyword}</span>'
                )
            # Highlight symbolicated resolution arrows
            summary_escaped = re.sub(
                r'→ (.+?) at (.+?:\d+)',
                r'→ <span style="color:#1a73e8;font-weight:700">\1</span> at <span style="color:#27ae60;font-style:italic">\2</span>',
                summary_escaped
            )
            summary_escaped = re.sub(
                r'→ ([^\n<]+)',
                r'→ <span style="color:#1a73e8;font-weight:700">\1</span>',
                summary_escaped
            )
            # Symbolication badge
            sym_badge = ''
            if freeze.get('symbolicated'):
                sym_badge = '<span style="margin-left:8px;padding:2px 6px;border-radius:4px;background:#e8f5e9;color:#2e7d32;font-size:10px;font-weight:600;">SYMBOLICATED</span>'
            else:
                sym_badge = '<span style="margin-left:8px;padding:2px 6px;border-radius:4px;background:#fff3e0;color:#e65100;font-size:10px;font-weight:600;">RAW</span>'

            links_html = f'<a href="crashes/{html.escape(freeze["raw_filename"])}" target="_blank" style="margin-left:8px;font-size:11px;color:#1a73e8;text-decoration:none;" title="Open raw log">&#128279; raw</a>'
            if freeze.get('sym_filename'):
                links_html += f' <a href="crashes/{html.escape(freeze["sym_filename"])}" target="_blank" style="margin-left:4px;font-size:11px;color:#2e7d32;text-decoration:none;" title="Open symbolicated log">&#128279; symbolicated</a>'

            freeze_details += f'''<details style="margin-bottom:8px;">
  <summary style="cursor:pointer;padding:8px 12px;background:#fafafa;border-radius:8px;font-size:13px;font-weight:500;">
    <span class="arrow">&#9654;</span> {html.escape(freeze['filename'])} ({freeze['full_lines']} lines)
    {sym_badge}
    {links_html}
  </summary>
  <pre style="margin:8px 0 0 0;padding:12px;background:#1a1a2e;color:#e0e0e0;border-radius:8px;font-size:11px;line-height:1.5;overflow-x:auto;max-height:400px;overflow-y:auto;">{summary_escaped}</pre>
</details>'''

        freeze_html = f'''<div class="section">
  <h2>Freeze / ANR Analysis</h2>
  <div style="margin-bottom:12px;font-size:13px;color:#666;">
    Freeze distribution by scenario (heartbeat timeout):
  </div>
  {freeze_timeline}
  {freeze_details}
</div>'''
    elif freeze_count > 0:
        freeze_html = f'''<div class="section">
  <h2>Freeze / ANR Analysis</h2>
  <div style="padding:16px;color:#e65100;font-size:13px;">
    {freeze_count} freeze(s) detected but no log files available.
  </div>
</div>'''

    # SVG chart
    svg_chart = generate_svg_chart(mem_points)

    # Build full HTML
    report_html = f'''<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>AGenUI SDK Stability Test Report</title>
<style>
  * {{ box-sizing: border-box; }}
  body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; margin: 0; padding: 24px; background: #f5f7fa; color: #333; }}
  .container {{ max-width: 960px; margin: 0 auto; }}
  h1 {{ color: #1a1a2e; margin-bottom: 4px; font-size: 24px; }}
  h2 {{ color: #1a1a2e; font-size: 16px; margin: 0 0 12px 0; }}
  .git-meta {{ display: flex; gap: 8px; flex-wrap: wrap; align-items: center; margin-bottom: 8px; }}
  .git-badge {{ padding: 3px 12px; border-radius: 12px; font-size: 12px; font-weight: 600; display: inline-block; }}
  .timestamp {{ color: #aaa; font-size: 13px; margin-bottom: 20px; }}
  .verdict {{ padding: 14px 24px; border-radius: 12px; margin-bottom: 24px; display: flex; align-items: center; gap: 12px; }}
  .verdict-icon {{ font-size: 28px; }}
  .verdict-text {{ font-size: 18px; font-weight: 700; }}
  .verdict-detail {{ font-size: 13px; opacity: 0.8; }}
  .cards {{ display: flex; gap: 16px; margin-bottom: 24px; flex-wrap: wrap; }}
  .card {{ background: #fff; border-radius: 14px; padding: 20px 28px; box-shadow: 0 2px 10px rgba(0,0,0,.07); flex: 1; min-width: 140px; }}
  .card .lbl {{ font-size: 11px; color: #aaa; text-transform: uppercase; letter-spacing: .5px; margin-bottom: 8px; }}
  .card .val {{ font-size: 32px; font-weight: 800; }}
  .card .unit {{ font-size: 13px; color: #aaa; margin-top: 4px; }}
  .section {{ background: #fff; border-radius: 14px; padding: 20px 24px; box-shadow: 0 2px 10px rgba(0,0,0,.07); margin-bottom: 20px; }}
  table {{ width: 100%; border-collapse: collapse; }}
  thead {{ background: #1a1a2e; color: #fff; }}
  th {{ padding: 10px 14px; text-align: left; font-size: 12px; font-weight: 600; }}
  th:first-child {{ border-radius: 8px 0 0 8px; }}
  th:last-child {{ border-radius: 0 8px 8px 0; }}
  td {{ padding: 9px 14px; border-bottom: 1px solid #f3f3f3; font-size: 13px; }}
  tr:last-child td {{ border-bottom: none; }}
  details summary::-webkit-details-marker {{ display: none; }}
  details summary {{ list-style: none; }}
  details summary .arrow {{ display: inline-block; transition: transform 0.2s ease; margin-right: 6px; }}
  details[open] summary .arrow {{ transform: rotate(90deg); }}
  .metric-grid {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(130px, 1fr)); gap: 12px; }}
  .metric-item {{ text-align: center; padding: 12px; background: #f8f9fa; border-radius: 10px; }}
  .metric-item .m-val {{ font-size: 22px; font-weight: 700; color: #1a1a2e; }}
  .metric-item .m-lbl {{ font-size: 11px; color: #aaa; margin-top: 4px; text-transform: uppercase; }}
  footer {{ text-align: center; margin-top: 32px; font-size: 12px; color: #ccc; }}
</style>
</head>
<body>
<div class="container">
  <h1>AGenUI SDK Stability Test Report</h1>
  <div class="git-meta">
    <span class="git-badge" style="background:#e8f0fe;color:#1a73e8;">{html.escape(git_branch)}</span>
    <span class="git-badge" style="background:#f0f0f0;color:#666;">{html.escape(git_commit)}</span>
    <span class="git-badge" style="background:#f5f0ff;color:#7c3aed;">{html.escape(platform)}</span>
    <span class="git-badge" style="background:#fff3e0;color:#e65100;">{html.escape(scenario)}</span>
  </div>
  <div class="timestamp">Start: {html.escape(start_time)} &nbsp;|&nbsp; End: {html.escape(end_time)} &nbsp;|&nbsp; Duration: {duration_min} min</div>

  <!-- Verdict Banner -->
  <div class="verdict" style="background:{verdict_bg};border:2px solid {verdict_color};">
    <div class="verdict-icon">{'&#10004;' if passed else '&#10006;'}</div>
    <div>
      <div class="verdict-text" style="color:{verdict_color};">{verdict_text}</div>
      <div class="verdict-detail">{'Zero crashes/freezes in ' + str(duration_min) + ' minutes (' + str(total_rounds) + ' rounds)' if passed else str(crash_count) + ' crash(es), ' + str(freeze_count) + ' freeze(s) detected'}</div>
    </div>
  </div>

  <!-- Summary Cards -->
  <div class="cards">
    <div class="card">
      <div class="lbl">Total Rounds</div>
      <div class="val" style="color:#2c3e50">{total_rounds}</div>
      <div class="unit">success rate: {success_rate:.1f}%</div>
    </div>
    <div class="card">
      <div class="lbl">Crashes</div>
      <div class="val" style="color:{'#e74c3c' if crash_count > 0 else '#27ae60'}">{crash_count}</div>
      <div class="unit">threshold: {crash_threshold}</div>
    </div>
    <div class="card">
      <div class="lbl">Freezes</div>
      <div class="val" style="color:{'#f39c12' if freeze_count > 0 else '#27ae60'}">{freeze_count}</div>
      <div class="unit">ANR / deadlock</div>
    </div>
    <div class="card">
      <div class="lbl">MTBF</div>
      <div class="val" style="color:#2c3e50;font-size:22px;">{mtbf_text}</div>
      <div class="unit">mean time between failures</div>
    </div>
    <div class="card">
      <div class="lbl">Memory Growth</div>
      <div class="val" style="color:{leak_color};font-size:26px;">{'+' if mem_growth >= 0 else ''}{mem_growth:.1f} MB</div>
      <div class="unit">{growth_rate:.1f} MB/hour ({leak_risk} risk)</div>
    </div>
  </div>

  <!-- Performance Metrics -->
  <div class="section">
    <h2>Performance Metrics</h2>
    <div class="metric-grid">
      <div class="metric-item"><div class="m-val">{avg_duration:.0f}</div><div class="m-lbl">Avg (ms)</div></div>
      <div class="metric-item"><div class="m-val">{p50}</div><div class="m-lbl">P50 (ms)</div></div>
      <div class="metric-item"><div class="m-val">{p95}</div><div class="m-lbl">P95 (ms)</div></div>
      <div class="metric-item"><div class="m-val">{p99}</div><div class="m-lbl">P99 (ms)</div></div>
      <div class="metric-item"><div class="m-val">{max_duration}</div><div class="m-lbl">Max (ms)</div></div>
      <div class="metric-item"><div class="m-val">{throughput:.0f}</div><div class="m-lbl">Rounds/min</div></div>
    </div>
  </div>

  <!-- Memory Analysis -->
  <div class="section">
    <h2>Memory Analysis</h2>
    <div style="display:flex;gap:24px;margin-bottom:16px;flex-wrap:wrap;">
      <div><span style="color:#aaa;font-size:12px;">START</span><br><strong>{mem_start:.1f} MB</strong></div>
      <div><span style="color:#aaa;font-size:12px;">END</span><br><strong>{mem_end:.1f} MB</strong></div>
      <div><span style="color:#aaa;font-size:12px;">PEAK</span><br><strong>{peak_mem:.1f} MB</strong></div>
      <div><span style="color:#aaa;font-size:12px;">GROWTH RATE</span><br><strong style="color:{leak_color}">{growth_rate:.1f} MB/hour</strong></div>
      <div><span style="color:#aaa;font-size:12px;">LEAK RISK</span><br><strong style="color:{leak_color}">{leak_risk}</strong></div>
    </div>
    <div style="overflow-x:auto;">
      {svg_chart}
    </div>
  </div>

  <!-- Scenario Breakdown -->
  <div class="section">
    <h2>Scenario Breakdown</h2>
    <table>
      <thead><tr><th>Scenario</th><th>Rounds</th><th>Success Rate</th><th>Avg Duration</th><th>Crashes</th></tr></thead>
      <tbody>{scenario_table_rows}</tbody>
    </table>
  </div>

  <!-- Crash Analysis -->
  {crash_html}

  <!-- Freeze / ANR Analysis -->
  {freeze_html}

  <footer>AGenUI Stability Test Framework &middot; generated by generate_report.py</footer>
</div>
</body>
</html>'''

    # Write report
    output_path = os.path.join(input_dir, 'report.html')
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(report_html)
    print(f'[Report] Generated: {output_path}')
    return output_path


def main():
    parser = argparse.ArgumentParser(description='Generate stability test HTML report')
    parser.add_argument('--input-dir', required=True,
                        help='Path to platform output directory (e.g., .../android/)')
    args = parser.parse_args()

    if not os.path.isdir(args.input_dir):
        print(f'[Report] Error: directory not found: {args.input_dir}')
        return 1

    generate_report(args.input_dir)
    return 0


if __name__ == '__main__':
    exit(main())
