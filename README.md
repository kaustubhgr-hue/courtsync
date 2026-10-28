# CourtSync

A basketball scoreboard from firmware to dashboard. Software only, no hardware.

```
 +-----------+  state.json   +--------+  POST /api/state  +------------+  poll 1s  +--------+
 | C engine  |-------------->| bridge |------------------>| Express    |<----------| React  |
 | (engine/) |  /dev/        | (py)   |                   | (web/)     |           | page   |
 +-----------+  scoreboard   +--------+                   +------------+           +--------+
       |        (--device)       ^
       v                         |
 +-----------------+  read       |
 | kernel module   |-------------+
 | (kernel/)       |
 +-----------------+
```

## Status (honest)

| Layer | What | Status |
|-------|------|--------|
| 1 | C engine (`engine/`) | **Done** - `make test` passes (49 checks); CLI, state.json and events.log smoke-tested on Windows (MSYS2 UCRT64 gcc); also builds on Ubuntu, and `--device /dev/scoreboard` verified there. |
| 2 | Linux kernel module (`kernel/`) | **Done** - built and loaded on Ubuntu 26.04 VM (kernel 7.0.0-38-generic): insmod, dmesg, `crw-rw-rw-` device, echo/cat round trip, engine `--device` writes, rmmod all verified |
| 3 | Python bridge (`bridge/`) | **Done** - tested on Windows (Python 3.13) against a mock HTTP server: state.json fallback, device preferred when present (stand-in file), empty/half-written JSON skipped, server down handled. Reading the real `/dev/scoreboard` verified in the full VM run below. |

**Full chain verified:** engine in an Ubuntu VM (`--device /dev/scoreboard`) -> kernel module -> bridge in the VM
-> Express API on the Windows host -> React dashboard in a Windows browser, updating live.
| 4 | Express + React dashboard (`web/`) | **Done** - API tests pass (5/5, `npm test`); `vite build` succeeds; full pipeline engine -> state.json -> bridge -> API -> dashboard verified live on Windows (desktop and phone width, waiting/live/offline states). |

## Layer 1: build and run

Needs gcc and make (on Windows: MSYS2 MinGW64 shell).

```
cd engine
make          # builds courtsync
make test     # builds and runs tests
./courtsync   # then type commands
```

Commands: `start`, `pause`, `tick [n]`, `score <home|away> <pts>`, `foul <home|away> <player 1-5>`,
`timeout <home|away>`, `next_quarter`, `status`, `quit`.
`tick` advances the simulated clock by n seconds (default 1) while the game is RUNNING.

Options: `--state <file>` (default `state.json`), `--log <file>` (default `events.log`),
`--device <path>` (also write the state line to that file, for Layer 2).

## Layer 2: kernel module (Linux VM only)

`kernel/scoreboard.c` registers a misc character device `/dev/scoreboard`.
A write replaces the stored state (max 4096 bytes, bigger writes get `EINVAL`);
a read returns the latest state. A mutex guards the buffer.

Target: Ubuntu 26.04 in VirtualBox (Secure Boot off). Setup once:

```
sudo apt install -y build-essential git
sudo apt install -y linux-headers-$(uname -r)
```

Build, load, test, unload:

```
cd kernel
make                              # produces scoreboard.ko
sudo insmod scoreboard.ko
sudo dmesg | tail -3              # "scoreboard: loaded, /dev/scoreboard ready"
ls -l /dev/scoreboard             # crw-rw-rw- 1 root root 10, <minor> ...
echo '{"test":1}' > /dev/scoreboard
cat /dev/scoreboard               # {"test":1}
sudo rmmod scoreboard
sudo dmesg | tail -1              # "scoreboard: unloaded"
```

Engine -> device:

```
cd engine && make
./courtsync --device /dev/scoreboard     # type: start, tick 5, score home 2
cat /dev/scoreboard                      # (second terminal) shows the engine's JSON
```

`dmesg` may also show "loading out-of-tree module taints kernel" and
"module verification failed" - both are normal for a self-built, unsigned module.

## Layer 3: bridge

Standard library only, no pip install. Every second it reads `/dev/scoreboard`
(or `engine/state.json` if the device doesn't exist), checks it is valid JSON,
and POSTs it to the web server. If the server is down it prints an error and retries.

```
python bridge/bridge.py                       # defaults: http://localhost:3000/api/state
python3 bridge/bridge.py --url 192.168.0.103:3000   # from a VM: host's LAN IP (/api/state is added)
```

VirtualBox 7 NAT blocks VMs from reaching the host's `localhost` via `10.0.2.2` by default.
Use the host's LAN IP (`ipconfig` on Windows), or power the VM off and run
`VBoxManage modifyvm <vm> --nat-localhostreachable1 on` to allow `10.0.2.2`.

Options: `--url`, `--device` (default `/dev/scoreboard`), `--state` (default `engine/state.json`), `--interval` (seconds).

## Layer 4: web dashboard

Node 20.19+ (tested with Node 24). Express API on port 3000, React page (Vite) on port 5173.

```
cd web
npm install
npm test             # API tests
npm run server       # terminal 1: API on http://localhost:3000
npm run dev          # terminal 2: dashboard on http://localhost:5173
```

API (CORS enabled):

| Method | Path | What |
|--------|------|------|
| POST | `/api/state` | bridge sends the engine state JSON (400 if it isn't engine-shaped) |
| GET | `/api/state` | latest state, or 404 before the first post |
| GET | `/api/events` | event history, newest first (last 100; the server collects them by `seq`, so it keeps more than the engine's 10-event ring; resets if the engine restarts) |

The page polls both GET endpoints every second and shows score, game clock, shot clock
(red at 5 or less), quarter, state, team fouls, timeouts, penalty and violation tags,
and the events feed. The pill at the top shows `waiting` / `live` / `offline`.

## Run everything with the kernel module (VM + Windows)

```
# Windows
cd web && npm run server
cd web && npm run dev                         # open http://localhost:5173
# Ubuntu VM
cd kernel && make && sudo insmod scoreboard.ko
cd engine && make && ./courtsync --device /dev/scoreboard
python3 bridge/bridge.py --url <windows-LAN-IP>:3000
```

## Run everything (Windows only, no kernel module)

Four terminals, from the project root:

```
cd web && npm run server
cd web && npm run dev
python bridge/bridge.py
cd engine && ./courtsync        # type commands; watch http://localhost:5173
```
