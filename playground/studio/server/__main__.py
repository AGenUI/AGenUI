"""Entry point: python -m playground.studio.server [--host HOST] [--port PORT]."""

from __future__ import annotations

import argparse
import socket
import threading
import time
import webbrowser


def _open_when_ready(url: str, port: int) -> None:
    """Poll the port until the server accepts connections, then open browser."""
    for _ in range(50):  # max ~5 s
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=0.2):
                webbrowser.open(url)
                return
        except (ConnectionRefusedError, OSError):
            time.sleep(0.1)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="AGenUI Studio - Local A2UI Generation Server"
    )
    parser.add_argument("--host", default=None, help="Bind host (default: from config or 0.0.0.0)")
    parser.add_argument("--port", type=int, default=None, help="Bind port (default: from config or 8765)")
    parser.add_argument("--no-browser", action="store_true", help="Do not auto-open browser")
    args = parser.parse_args()

    from .config import get_lan_ip, load_config

    cfg = load_config()
    host = args.host or cfg.host
    port = args.port or cfg.port
    lan_ip = get_lan_ip()

    print("AGenUI Studio starting...")
    print(f"  Local:   http://127.0.0.1:{port}")
    print(f"  Network: http://{lan_ip}:{port}")
    print()

    if not args.no_browser:
        threading.Thread(
            target=_open_when_ready,
            args=(f"http://127.0.0.1:{port}", port),
            daemon=True,
        ).start()

    import uvicorn

    uvicorn.run(
        "playground.studio.server.server:app",
        host=host,
        port=port,
        log_level="info",
    )


if __name__ == "__main__":
    main()
