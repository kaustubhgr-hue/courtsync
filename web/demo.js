// demo.js - DEMO=1 mode for the hosted site. Runs the REAL C engine (engine/courtsync),
// types a simulated game into it once a second, and posts the engine's state.json to this
// server - the same path the Python bridge uses, just without the kernel module.
import { spawn } from "node:child_process";
import { readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";

const HERE = path.dirname(fileURLToPath(import.meta.url));
const ENGINE = path.join(HERE, "..", "engine", process.platform === "win32" ? "courtsync.exe" : "courtsync");
// Per-process file names, so two servers on one machine never share an engine's files.
const STATE = path.join(tmpdir(), `courtsync-demo-${process.pid}.json`);
const LOG = path.join(tmpdir(), `courtsync-demo-${process.pid}.log`);
const FLAG_QUARTER_OVER = 1 << 3;
const SHOW_FINAL_SECS = 20;            // keep "FINAL" on screen before starting a new game

const pick = (list) => list[Math.floor(Math.random() * list.length)];

export function startDemo(port, token) {
  let engine;
  let endedFor = 0;

  function startEngine() {
    engine = spawn(ENGINE, ["--state", STATE, "--log", LOG], { stdio: ["pipe", "ignore", "inherit"] });
    engine.on("error", (e) => console.error(`demo: cannot run ${ENGINE}: ${e.message}`));
  }
  const send = (cmd) => engine.stdin.write(cmd + "\n");

  startEngine();
  console.log("demo: simulated game running");

  setInterval(async () => {
    let s;
    try {
      s = JSON.parse(readFileSync(STATE, "utf8"));
    } catch {
      return;                          // engine not started yet, or mid-write: try next second
    }

    // Post what the engine wrote (like the bridge does).
    try {
      await fetch(`http://127.0.0.1:${port}/api/state`, {
        method: "POST",
        headers: { "Content-Type": "application/json", ...(token ? { "X-Token": token } : {}) },
        body: JSON.stringify(s),
      });
    } catch (e) {
      console.error("demo: post failed:", e.message);
    }

    // Decide the next command, like a scorekeeper at the table.
    if (s.state === "ENDED") {
      if (++endedFor >= SHOW_FINAL_SECS) {          // new game
        endedFor = 0;
        send("quit");
        startEngine();
      }
    } else if (s.state === "TIMEOUT") {
      if (Math.random() < 0.15) send("start");      // timeouts last a few seconds
    } else if (s.state === "PAUSED") {
      if (s.flags & FLAG_QUARTER_OVER) send("next_quarter");
      send("start");                                 // after next_quarter in Q4 this is rejected (ENDED)
    } else {                                         // RUNNING
      const r = Math.random();
      if (r < 0.045) send(`score ${pick(["home", "away"])} ${pick([2, 2, 2, 3, 3, 1])}`);
      else if (r < 0.065) send(`foul ${pick(["home", "away"])} ${1 + Math.floor(Math.random() * 5)}`);
      else if (r < 0.068) send(`timeout ${pick(["home", "away"])}`);
      send("tick 1");                                // one simulated second per real second
    }
  }, 1000);
}
