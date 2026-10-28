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
| 1 | C engine (`engine/`) | **Done** - `make test` passes (49 checks); CLI, state.json and events.log smoke-tested on Windows (MSYS2 UCRT64 gcc). `--device` not tested yet (needs Layer 2). |
| 2 | Linux kernel module (`kernel/`) | Planned |
| 3 | Python bridge (`bridge/`) | Planned |
| 4 | Express + React dashboard (`web/`) | Planned |

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
