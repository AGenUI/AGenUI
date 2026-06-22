#!/usr/bin/env python3
"""
tests/integration/parse_reports.py
AGenUI Test Report Parser (Python3)

Parses multi-platform JUnit XML test reports and generates an HTML summary
report with detailed test case listings.

Usage:
    python3 parse_reports.py --reports-dir <dir> --output <html_file>
    python3 parse_reports.py --generate-index --runs-dir <dir> --output <html_file>
"""
import argparse
import json
import os
import sys
import glob
import re
from datetime import datetime
from xml.etree import ElementTree as ET


def build_testid_doc_map(project_root: str) -> dict:
    """Scan markdown files under test_fixtures directory, build test_id -> relative doc path mapping.

    Each markdown file contains a `| Related Tests | COMP-01, INIT-01 |` line,
    from which test IDs are extracted and mapped to the corresponding document relative path.
    """
    fixtures_dir = os.path.join(project_root, 'playground', 'resource', 'test_fixtures')
    mapping = {}
    if not os.path.isdir(fixtures_dir):
        return mapping
    for dirpath, _, filenames in os.walk(fixtures_dir):
        for fname in filenames:
            if not fname.endswith('.md'):
                continue
            filepath = os.path.join(dirpath, fname)
            try:
                with open(filepath, encoding='utf-8', errors='ignore') as fh:
                    for line in fh:
                        m = re.search(r'\|\s*Related Tests\s*\|\s*(.+?)\s*\|', line)
                        if m:
                            ids = re.findall(r'([A-Z]+-\d+)', m.group(1))
                            rel = os.path.relpath(filepath, project_root)
                            for tid in ids:
                                mapping[tid] = rel
                            break
            except OSError:
                continue
    return mapping


def extract_test_id(name: str, classname: str = '') -> str:
    """Extract standardized test ID (e.g., COMP-01) from test case name.

    Supports naming styles across three platforms:
      - Harmony: COMP-01_text_only_render  (with hyphen)
      - Android: testINIT03_..., testSTREAM01_...  (no hyphen)
      - Android: testRender_01_textOnly  (ComponentRenderTest special format)
      - iOS:     testCOMP01_..., testINIT01_...  (no hyphen)
    """
    prefixes = r'INIT|SURFACE|MULTI|COMP|SKILL|PLATFORM|STREAM|SYNC|FUNC|SCENE'
    # 1. Hyphenated format (Harmony): COMP-01_...
    m = re.search(rf'({prefixes})-(\d+)', name, re.IGNORECASE)
    if m:
        return f'{m.group(1).upper()}-{m.group(2).zfill(2)}'
    # 2. No-hyphen format (Android/iOS): testINIT01_..., testSTREAM05_...
    m = re.search(rf'({prefixes})(\d+)', name, re.IGNORECASE)
    if m:
        return f'{m.group(1).upper()}-{m.group(2).zfill(2)}'
    # 3. Android ComponentRenderTest special format: testRender_01_textOnly
    m = re.search(r'[Rr]ender_(\d+)', name)
    if m:
        return f'COMP-{m.group(1).zfill(2)}'
    return ''


def parse_junit_xmls(xml_dir: str):
    """Parse all JUnit XML files in directory, return (total, passed, failed, errors, cases)"""
    if not os.path.isdir(xml_dir):
        return 0, 0, 0, 0, []

    xml_files = glob.glob(os.path.join(xml_dir, '**', '*.xml'), recursive=True)
    total = passed = failed = errors = 0
    cases = []

    for xml_file in xml_files:
        try:
            tree = ET.parse(xml_file)
            root = tree.getroot()
        except ET.ParseError:
            continue

        suites = []
        if root.tag == 'testsuites':
            suites = list(root)
        elif root.tag == 'testsuite':
            suites = [root]

        for suite in suites:
            if suite.tag != 'testsuite':
                continue
            for tc in suite.findall('testcase'):
                name = tc.get('name', '')
                classname = tc.get('classname', '')
                time_val = tc.get('time', '0')
                failure = tc.find('failure')
                error = tc.find('error')
                skipped = tc.find('skipped')
                total += 1
                if failure is not None:
                    status = 'FAIL'
                    message = (failure.get('message') or failure.text or '')[:200]
                    failed += 1
                elif error is not None:
                    status = 'ERROR'
                    message = (error.get('message') or error.text or '')[:200]
                    errors += 1
                elif skipped is not None:
                    status = 'SKIP'
                    message = ''
                else:
                    status = 'PASS'
                    message = ''
                    passed += 1
                cases.append({
                    'classname': classname,
                    'name': name,
                    'status': status,
                    'time': time_val,
                    'message': message,
                })

    return total, passed, failed, errors, cases


def parse_raw_output(txt_file: str):
    """Extract test results from raw_output.txt (fallback for Harmony/Android/iOS raw output)"""
    cases = []
    total = passed = failed = errors = 0
    if not os.path.isfile(txt_file):
        return total, passed, failed, errors, cases

    with open(txt_file, encoding='utf-8', errors='ignore') as f:
        content = f.read()

    # Try HarmonyOS OHOS_REPORT format
    if 'OHOS_REPORT_STATUS_CODE:' in content:
        total, passed, failed, errors, cases = _parse_harmony_output(content)
        return total, passed, failed, errors, cases

    # Try iOS xcodebuild test output format
    if 'Test Case' in content and ('.xctest' in content or 'XCTest' in content or "passed (" in content):
        total, passed, failed, errors, cases = _parse_ios_xcodebuild_output(content)
        return total, passed, failed, errors, cases

    # Match Android instrument output format
    for m in re.finditer(r'INSTRUMENTATION_STATUS_CODE:\s*(-?\d+)', content):
        code = int(m.group(1))
        total += 1
        if code == 1:
            passed += 1
            cases.append({'classname': '', 'name': f'test_{len(cases)+1}', 'status': 'PASS', 'time': '0', 'message': ''})
        elif code == -1 or code == -2:
            failed += 1
            cases.append({'classname': '', 'name': f'test_{len(cases)+1}', 'status': 'FAIL', 'time': '0', 'message': ''})

    return total, passed, failed, errors, cases


def _parse_harmony_output(content: str):
    """Parse HarmonyOS ohosTest raw output format"""
    cases = []
    total = passed = failed = errors = 0

    # Split into blocks by blank lines
    blocks = re.split(r'\n\s*\n', content)

    for block in blocks:
        # Only process blocks containing result status code (CODE: 0 or -1/-2, skip CODE: 1 which means test starting)
        code_match = re.search(r'OHOS_REPORT_STATUS_CODE:\s*(-?\d+)', block)
        if not code_match:
            continue
        code = int(code_match.group(1))
        # CODE: 1 means test is starting, not a result
        if code == 1:
            continue

        # Extract class name
        class_match = re.search(r'OHOS_REPORT_STATUS:\s*class=(\S+)', block)
        classname = class_match.group(1) if class_match else ''

        # Extract test name
        test_match = re.search(r'OHOS_REPORT_STATUS:\s*test=(.+)', block)
        testname = test_match.group(1).strip() if test_match else f'test_{total + 1}'

        # Extract duration (milliseconds)
        time_match = re.search(r'OHOS_REPORT_STATUS:\s*consuming=(\d+)', block)
        time_ms = int(time_match.group(1)) if time_match else 0
        time_sec = f'{time_ms / 1000:.3f}'

        total += 1
        if code == 0:
            passed += 1
            cases.append({'classname': classname, 'name': testname, 'status': 'PASS', 'time': time_sec, 'message': ''})
        else:
            # -1 or -2: failure/error
            failed += 1
            # Try to get error message from stream field
            stream_match = re.search(r'OHOS_REPORT_STATUS:\s*stream=(.+)', block)
            message = stream_match.group(1).strip()[:200] if stream_match and stream_match.group(1).strip() else ''
            cases.append({'classname': classname, 'name': testname, 'status': 'FAIL', 'time': time_sec, 'message': message})

    return total, passed, failed, errors, cases


def _parse_ios_xcodebuild_output(content: str):
    """Parse iOS xcodebuild test output format.

    Matches format:
      Test Case '-[AGenUITests.InitializationTest testINIT01_sdkInitialized]' passed (0.001 seconds).
      Test Case '-[AGenUITests.StreamTest testSTREAM01_smallChunkButtonScene]' failed (0.118 seconds).
      Test Case '-[AGenUITests.FunctionCallTest testSKILL07_validateRequired]' skipped (0.004 seconds).
    """
    cases = []
    total = passed = failed = errors = 0

    # Match Test Case lines (supports passed/failed/skipped states, XCTSkip normalized to SKIP)
    pattern = re.compile(
        r"Test Case '-\[(\S+?)\.(\S+)\s+(\S+)\]' (passed|failed|skipped) \((\d+\.\d+) seconds\)\."
    )

    for m in pattern.finditer(content):
        module_class = m.group(1)  # e.g. "AGenUITests"
        classname = m.group(2)      # e.g. "InitializationTest"
        testname = m.group(3)       # e.g. "testINIT01_sdkInitialized"
        status_str = m.group(4)     # "passed" / "failed" / "skipped"
        time_sec = m.group(5)       # e.g. "0.001"

        total += 1
        if status_str == 'passed':
            passed += 1
            cases.append({
                'classname': classname,
                'name': testname,
                'status': 'PASS',
                'time': time_sec,
                'message': ''
            })
        elif status_str == 'skipped':
            # XCTSkip: not counted as passed/failed, but counted in total, status shown as SKIP
            cases.append({
                'classname': classname,
                'name': testname,
                'status': 'SKIP',
                'time': time_sec,
                'message': ''
            })
        else:
            failed += 1
            # Try to get error message from context
            message = ''
            err_pattern = re.compile(
                rf"error:.*\[{re.escape(module_class)}\.{re.escape(classname)} {re.escape(testname)}\].*?:\s*(.+)",
                re.IGNORECASE
            )
            err_match = err_pattern.search(content)
            if err_match:
                message = err_match.group(1).strip()[:200]
            cases.append({
                'classname': classname,
                'name': testname,
                'status': 'FAIL',
                'time': time_sec,
                'message': message
            })

    return total, passed, failed, errors, cases


def render_status_badge(status: str) -> str:
    colors = {
        'PASS': ('#e8f8f0', '#27ae60'),
        'FAIL': ('#fde8e8', '#e74c3c'),
        'ERROR': ('#fff3e0', '#e67e22'),
        'SKIP': ('#f0f0f0', '#999'),
        'N/A': ('#f0f0f0', '#999'),
    }
    bg, fg = colors.get(status, ('#f0f0f0', '#999'))
    return f'<span style="background:{bg};color:{fg};padding:2px 10px;border-radius:10px;font-size:12px;font-weight:700">{status}</span>'


def _render_git_meta_html(git_meta: dict) -> str:
    """Generate HTML block for git metadata"""
    if not git_meta:
        return ''

    git_info = git_meta.get('git', {})
    branch = git_info.get('branch', '')
    commit_short = git_info.get('commit_short', '')
    dirty = git_info.get('dirty', False)
    run_key = git_meta.get('run_key', '')

    parts = []
    if branch:
        parts.append(
            f'<span class="git-badge" style="background:#e8f0fe;color:#1a73e8">{branch}</span>'
        )
    if commit_short:
        parts.append(
            f'<span class="git-badge" style="background:#f3e8fd;color:#7c3aed;font-family:\'SF Mono\',Menlo,monospace">{commit_short}</span>'
        )
    if dirty:
        parts.append(
            '<span class="git-badge" style="background:#fef3c7;color:#d97706">dirty</span>'
        )
    else:
        parts.append(
            '<span class="git-badge" style="background:#e8f8f0;color:#27ae60">clean</span>'
        )
    if run_key:
        parts.append(
            f'<span style="color:#bbb;font-size:11px;margin-left:4px">{run_key}</span>'
        )

    return f'<div class="git-meta">{" ".join(parts)}</div>'


def render_html(platforms_data: list, output_path: str, doc_map: dict = None,
                project_root: str = '', git_meta: dict = None):
    """Generate HTML report.

    Args:
        platforms_data: Test data for each platform
        output_path: HTML output path
        doc_map: test_id -> doc relative path mapping (optional)
        project_root: Project root directory (for computing relative paths)
        git_meta: Git metadata dictionary (optional)
    """
    timestamp = datetime.now().strftime('%Y-%m-%d %H:%M:%S')

    grand_total = sum(d['total'] for d in platforms_data)
    grand_passed = sum(d['passed'] for d in platforms_data)
    grand_failed = sum(d['failed'] + d['errors'] for d in platforms_data)
    pass_rate = round(grand_passed * 100 / grand_total, 1) if grand_total > 0 else 0

    pass_rate_color = '#27ae60' if grand_failed == 0 else '#e74c3c'
    fail_val_color = '#95a5a6' if grand_failed == 0 else '#e74c3c'

    git_meta_html = _render_git_meta_html(git_meta)

    # footer run key
    run_key = git_meta.get('run_key', '') if git_meta else ''
    footer_extra = f' &nbsp;&middot;&nbsp; Run: {run_key}' if run_key else ''

    # Generate test case tables per platform
    platform_sections = ''
    for d in platforms_data:
        pname = d['platform']
        cases = d['cases']
        if not cases:
            section = f'<p style="color:#999;font-style:italic">No test case data available</p>'
        else:
            rows = ''
            for c in cases:
                badge = render_status_badge(c['status'])
                msg = f'<span style="color:#e74c3c;font-size:12px">{c["message"]}</span>' if c['message'] else ''
                # Build doc link
                doc_link = ''
                if doc_map:
                    tid = extract_test_id(c['name'], c.get('classname', ''))
                    if tid and tid in doc_map:
                        doc_rel_root = doc_map[tid]  # relative to project root
                        doc_link = f' <a href="#" data-doc="{doc_rel_root}" target="_blank" title="{tid} test spec" class="doc-link">?</a>'
                rows += f'<tr><td>{c["classname"]}</td><td>{c["name"]}{doc_link}</td><td>{badge}</td><td>{c["time"]}s</td><td>{msg}</td></tr>'
            section = f'''
            <table>
              <thead><tr><th>Class</th><th>Test Case</th><th>Status</th><th>Duration</th><th>Message</th></tr></thead>
              <tbody>{rows}</tbody>
            </table>'''
        pstatus = 'PASS' if (d['failed'] + d['errors']) == 0 and d['total'] > 0 else ('N/A' if d['total'] == 0 else 'FAIL')
        badge = render_status_badge(pstatus)
        platform_sections += f'''
      <details open>
        <summary style="cursor:pointer;padding:12px 0;font-size:16px;font-weight:700;list-style:none">
          <span class="arrow">&#9654;</span>{pname} &#8212; {d['total']} cases / {d['passed']} passed / {d['failed']+d['errors']} failed &nbsp;{badge}
        </summary>
        {section}
      </details>
      <hr style="border:none;border-top:1px solid #eee;margin:8px 0">'''

    html = f'''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>AGenUI Integration Test Report</title>
<style>
  * {{ box-sizing: border-box; }}
  body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; margin: 0; padding: 24px; background: #f5f7fa; color: #333; }}
  .container {{ max-width: 960px; margin: 0 auto; }}
  h1 {{ color: #1a1a2e; margin-bottom: 4px; font-size: 24px; }}
  .git-meta {{ display: flex; gap: 8px; flex-wrap: wrap; align-items: center; margin-bottom: 8px; }}
  .git-badge {{ padding: 3px 12px; border-radius: 12px; font-size: 12px; font-weight: 600; display: inline-block; }}
  .timestamp {{ color: #aaa; font-size: 13px; margin-bottom: 28px; }}
  .cards {{ display: flex; gap: 16px; margin-bottom: 32px; flex-wrap: wrap; }}
  .card {{ background: #fff; border-radius: 14px; padding: 20px 28px; box-shadow: 0 2px 10px rgba(0,0,0,.07); flex: 1; min-width: 150px; }}
  .card .lbl {{ font-size: 12px; color: #aaa; text-transform: uppercase; letter-spacing: .5px; margin-bottom: 8px; }}
  .card .val {{ font-size: 36px; font-weight: 800; }}
  table {{ width: 100%; border-collapse: collapse; background: #fff; border-radius: 12px; overflow: hidden; box-shadow: 0 2px 8px rgba(0,0,0,.07); margin-top: 8px; }}
  thead {{ background: #1a1a2e; color: #fff; }}
  th {{ padding: 12px 16px; text-align: left; font-size: 13px; font-weight: 600; }}
  td {{ padding: 10px 16px; border-bottom: 1px solid #f3f3f3; font-size: 13px; }}
  tr:last-child td {{ border-bottom: none; }}
  details summary::-webkit-details-marker {{ display: none; }}
  details summary {{ list-style: none; }}
  details summary .arrow {{ display: inline-block; transition: transform 0.2s ease; margin-right: 6px; }}
  details[open] summary .arrow {{ transform: rotate(90deg); }}
  .doc-link {{ display: inline-block; width: 16px; height: 16px; line-height: 16px; text-align: center; border-radius: 50%; background: #e8f0fe; color: #1a73e8; font-size: 11px; font-weight: 700; text-decoration: none; margin-left: 6px; vertical-align: middle; }}
  .doc-link:hover {{ background: #1a73e8; color: #fff; }}
  footer {{ text-align: center; margin-top: 32px; font-size: 12px; color: #ccc; }}
</style>
</head>
<body>
<div class="container">
  <h1>AGenUI Integration Test Report</h1>
  {git_meta_html}
  <div class="timestamp">Generated: {timestamp}</div>

  <div class="cards">
    <div class="card"><div class="lbl">Total Cases</div><div class="val" style="color:#2c3e50">{grand_total}</div></div>
    <div class="card"><div class="lbl">Passed</div><div class="val" style="color:#27ae60">{grand_passed}</div></div>
    <div class="card"><div class="lbl">Failed/Errors</div><div class="val" style="color:{fail_val_color}">{grand_failed}</div></div>
    <div class="card"><div class="lbl">Pass Rate</div><div class="val" style="color:{pass_rate_color}">{pass_rate}%</div></div>
  </div>

  {platform_sections}

  <footer>AGenUI Test Automation Framework &nbsp;&middot;&nbsp; generated by parse_reports.py{footer_extra}</footer>
</div>
<script>
(function(){{
  var p=window.location.pathname,i=p.indexOf('/reports/');
  if(i===-1)return;
  var root=p.substring(0,i+1);
  document.querySelectorAll('a.doc-link[data-doc]').forEach(function(a){{
    a.href=root+a.getAttribute('data-doc');
  }});
}})();
</script>
</body>
</html>'''

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(html)


def generate_index_html(runs_dir: str, output_path: str):
    """Scan subdirectories under runs/, read metadata.json and generate history index page"""
    if not os.path.isdir(runs_dir):
        print(f'[Index] runs directory does not exist: {runs_dir}')
        return

    runs = []
    for entry in os.listdir(runs_dir):
        run_dir = os.path.join(runs_dir, entry)
        if not os.path.isdir(run_dir):
            continue
        meta_path = os.path.join(run_dir, 'metadata.json')
        summary_path = os.path.join(run_dir, 'summary.html')
        meta = {}
        if os.path.isfile(meta_path):
            try:
                with open(meta_path, encoding='utf-8') as f:
                    meta = json.load(f)
            except (json.JSONDecodeError, OSError):
                pass

        # Even if metadata.json is missing, list the directory
        runs.append({
            'dir_name': entry,
            'run_key': meta.get('run_key', entry),
            'branch': meta.get('git', {}).get('branch', '-'),
            'commit_short': meta.get('git', {}).get('commit_short', '-'),
            'dirty': meta.get('git', {}).get('dirty', False),
            'timestamp': meta.get('timestamp', '-'),
            'platforms_run': meta.get('platforms_run', []),
            'exit_codes': meta.get('exit_codes', {}),
            'has_summary': os.path.isfile(summary_path),
        })

    # Sort by timestamp descending
    runs.sort(key=lambda r: r['timestamp'], reverse=True)

    # Generate table rows
    rows = ''
    for r in runs:
        # Platform results
        platform_badges = ''
        for p in ['android', 'ios', 'harmony']:
            if p in r['platforms_run']:
                code = r['exit_codes'].get(p, -1)
                if code == 0:
                    platform_badges += f'<span style="background:#e8f8f0;color:#27ae60;padding:1px 8px;border-radius:8px;font-size:11px;font-weight:600;margin-right:4px">{p}</span>'
                else:
                    platform_badges += f'<span style="background:#fde8e8;color:#e74c3c;padding:1px 8px;border-radius:8px;font-size:11px;font-weight:600;margin-right:4px">{p}</span>'
        if not platform_badges:
            platform_badges = '<span style="color:#ccc">-</span>'

        # Link
        link = ''
        if r['has_summary']:
            rel_path = f'runs/{r["dir_name"]}/summary.html'
            link = f'<a href="{rel_path}" style="color:#1a73e8;text-decoration:none;font-weight:600">View Report</a>'
        else:
            link = '<span style="color:#ccc">No Report</span>'

        # dirty mark
        dirty_mark = ' <span style="color:#d97706;font-size:10px">dirty</span>' if r['dirty'] else ''

        rows += f'''<tr>
  <td style="font-family:'SF Mono',Menlo,monospace;font-size:12px">{r['run_key']}</td>
  <td><span style="background:#e8f0fe;color:#1a73e8;padding:2px 8px;border-radius:8px;font-size:12px;font-weight:600">{r['branch']}</span></td>
  <td style="font-family:'SF Mono',Menlo,monospace">{r['commit_short']}{dirty_mark}</td>
  <td>{r['timestamp']}</td>
  <td>{platform_badges}</td>
  <td>{link}</td>
</tr>'''

    html = f'''<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>AGenUI Test History</title>
<style>
  * {{ box-sizing: border-box; }}
  body {{ font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; margin: 0; padding: 24px; background: #f5f7fa; color: #333; }}
  .container {{ max-width: 1100px; margin: 0 auto; }}
  h1 {{ color: #1a1a2e; margin-bottom: 4px; font-size: 24px; }}
  .subtitle {{ color: #aaa; font-size: 13px; margin-bottom: 28px; }}
  table {{ width: 100%; border-collapse: collapse; background: #fff; border-radius: 12px; overflow: hidden; box-shadow: 0 2px 8px rgba(0,0,0,.07); }}
  thead {{ background: #1a1a2e; color: #fff; }}
  th {{ padding: 12px 16px; text-align: left; font-size: 13px; font-weight: 600; }}
  td {{ padding: 10px 16px; border-bottom: 1px solid #f3f3f3; font-size: 13px; }}
  tr:last-child td {{ border-bottom: none; }}
  tr:hover td {{ background: #f9f9fb; }}
  footer {{ text-align: center; margin-top: 32px; font-size: 12px; color: #ccc; }}
</style>
</head>
<body>
<div class="container">
  <h1>AGenUI Test Run History</h1>
  <div class="subtitle">{len(runs)} run records &nbsp;&middot;&nbsp; Last updated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</div>

  <table>
    <thead>
      <tr>
        <th>Run Key</th>
        <th>Branch</th>
        <th>Commit</th>
        <th>Timestamp</th>
        <th>Platforms</th>
        <th>Report</th>
      </tr>
    </thead>
    <tbody>
      {rows if rows else '<tr><td colspan="6" style="text-align:center;color:#ccc;padding:40px">No run records yet</td></tr>'}
    </tbody>
  </table>

  <footer>AGenUI Test Automation Framework</footer>
</div>
</body>
</html>'''

    os.makedirs(os.path.dirname(output_path) if os.path.dirname(output_path) else '.', exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write(html)
    print(f'[Index] Index page generated: {output_path} ({len(runs)} records)')


def main():
    parser = argparse.ArgumentParser(description='AGenUI Test Report Generator')
    parser.add_argument('--reports-dir', default='reports', help='Multi-platform reports root directory')
    parser.add_argument('--output', default='reports/summary.html', help='HTML output path')
    parser.add_argument('--project-root', default=None, help='Project root directory (for locating test spec docs and generating doc links)')
    parser.add_argument('--metadata-json', default=None, help='Git metadata JSON string')
    parser.add_argument('--generate-index', action='store_true', help='Generate history run index page')
    parser.add_argument('--runs-dir', default=None, help='Runs directory path (for index mode)')
    args = parser.parse_args()

    # Index generation mode
    if args.generate_index:
        runs_dir = os.path.abspath(args.runs_dir) if args.runs_dir else os.path.abspath('reports/runs')
        output_path = os.path.abspath(args.output)
        generate_index_html(runs_dir, output_path)
        return

    # Normal report generation mode
    reports_dir = os.path.abspath(args.reports_dir)
    output_path = os.path.abspath(args.output)

    # Parse git metadata
    git_meta = None
    if args.metadata_json:
        try:
            git_meta = json.loads(args.metadata_json)
        except json.JSONDecodeError:
            print(f'[WARN] Failed to parse metadata-json, skipping git metadata embedding')

    platforms = [
        ('Android', os.path.join(reports_dir, 'android')),
        ('iOS', os.path.join(reports_dir, 'ios')),
        ('Harmony', os.path.join(reports_dir, 'harmony')),
    ]

    platforms_data = []
    for name, xml_dir in platforms:
        total, passed, failed, errors, cases = parse_junit_xmls(xml_dir)
        # If no XML found, try raw_output.txt as fallback
        if total == 0:
            raw_txt = os.path.join(xml_dir, 'raw_output.txt')
            total, passed, failed, errors, cases = parse_raw_output(raw_txt)
        platforms_data.append({
            'platform': name,
            'total': total,
            'passed': passed,
            'failed': failed,
            'errors': errors,
            'cases': cases,
        })
        print(f'[{name}] total={total} passed={passed} failed={failed} errors={errors}')

    # Build test ID -> doc path mapping
    doc_map = {}
    project_root = ''
    if args.project_root:
        project_root = os.path.abspath(args.project_root)
        doc_map = build_testid_doc_map(project_root)
        if doc_map:
            print(f'[Doc] Loaded {len(doc_map)} test spec doc mappings')

    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    render_html(platforms_data, output_path, doc_map=doc_map,
                project_root=project_root, git_meta=git_meta)
    print(f'Report generated: {output_path}')


if __name__ == '__main__':
    main()
