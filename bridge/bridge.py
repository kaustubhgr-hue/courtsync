"""bridge.py - reads the scoreboard state once a second and POSTs it to the web server.

Source: /dev/scoreboard (kernel module). If that device doesn't exist,
falls back to engine/state.json. Uses only the Python standard library.

Usage: python bridge.py [--url URL] [--device PATH] [--state PATH] [--interval SECS] [--token T]
--url may be just the server address (e.g. 192.168.0.103:3000); /api/state is added.
"""
import argparse
import json
import os
import time
import urllib.error
import urllib.parse
import urllib.request

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_STATE = os.path.normpath(os.path.join(HERE, "..", "engine", "state.json"))


def read_state(device, state_file):
    """Return (source_path, parsed_json) or (source_path, None) if nothing valid to send."""
    path = device if os.path.exists(device) else state_file
    try:
        with open(path, "r", encoding="utf-8") as f:
            text = f.read().strip()
    except OSError as e:
        print(f"read failed: {path}: {e}")
        return path, None
    if not text:
        return path, None                  # device loaded but engine hasn't written yet
    try:
        return path, json.loads(text)      # validate so we never post a half-written file
    except json.JSONDecodeError:
        print(f"skipping invalid JSON from {path}")
        return path, None


def full_url(url):
    """Accept 'http://host:3000' or a full URL; add /api/state when no path is given."""
    if "://" not in url:
        url = "http://" + url
    if urllib.parse.urlparse(url).path in ("", "/"):
        url = url.rstrip("/") + "/api/state"
    return url


def post_state(url, state, token=None):
    """POST state as JSON. Returns the HTTP status code; raises on network errors."""
    body = json.dumps(state).encode("utf-8")
    headers = {"Content-Type": "application/json"}
    if token:
        headers["X-Token"] = token         # required by a hosted server that has POST_TOKEN set
    req = urllib.request.Request(url, data=body, method="POST", headers=headers)
    with urllib.request.urlopen(req, timeout=3) as resp:
        return resp.status


def main():
    p = argparse.ArgumentParser(description="CourtSync bridge")
    p.add_argument("--url", default="http://localhost:3000/api/state")
    p.add_argument("--device", default="/dev/scoreboard")
    p.add_argument("--state", default=DEFAULT_STATE)
    p.add_argument("--interval", type=float, default=1.0)
    p.add_argument("--token", default=os.environ.get("COURTSYNC_TOKEN"),
                   help="X-Token for a hosted server (or set COURTSYNC_TOKEN)")
    args = p.parse_args()
    args.url = full_url(args.url)

    print(f"bridge: device={args.device} fallback={args.state} -> {args.url}")
    while True:
        source, state = read_state(args.device, args.state)
        if state is not None:
            try:
                status = post_state(args.url, state, args.token)
                print(f"posted from {source}: HTTP {status}  "
                      f"Q{state.get('quarter')} {state.get('state')} clock {state.get('clock')}")
            except urllib.error.HTTPError as e:              # server answered, but with an error
                print(f"post rejected by {args.url}: HTTP {e.code}")
            except (urllib.error.URLError, OSError) as e:
                print(f"post failed (server down?) {args.url}: {e}")   # keep running, retry next tick
        time.sleep(args.interval)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("bridge stopped")
