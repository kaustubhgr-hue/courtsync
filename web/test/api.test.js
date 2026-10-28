// api.test.js - run with `npm test`. Starts the API on a random port and calls it with fetch.
import { test, before, after } from "node:test";
import assert from "node:assert/strict";
import { createApp } from "../app.js";

let server, base;

before(() => new Promise((done) => {
  server = createApp().listen(0, () => {
    base = `http://127.0.0.1:${server.address().port}`;
    done();
  });
}));
after(() => server.close());

const post = (body) => fetch(`${base}/api/state`, {
  method: "POST",
  headers: { "Content-Type": "application/json" },
  body: JSON.stringify(body),
});

// Minimal engine-shaped state with the given event seq numbers.
const state = (seqs, score = 0) => ({
  state: "RUNNING", quarter: 1, clock: 600, shot_clock: 24, flags: 0,
  teams: [{ name: "HOME", score }, { name: "AWAY", score: 0 }],
  events: seqs.map((n) => ({ seq: n, text: `event ${n}` })),
});

test("GET /api/state is 404 before any post", async () => {
  assert.equal((await fetch(`${base}/api/state`)).status, 404);
});

test("POST rejects bad bodies", async () => {
  assert.equal((await post({ hello: 1 })).status, 400);
});

test("POST then GET returns latest state", async () => {
  assert.equal((await post(state([1, 2], 5))).status, 200);
  const res = await fetch(`${base}/api/state`);
  assert.equal(res.headers.get("access-control-allow-origin"), "*");   // CORS on
  const s = await res.json();
  assert.equal(s.teams[0].score, 5);
  assert.ok(s.received_at > 0);
});

test("events are de-duplicated by seq and returned newest first", async () => {
  await post(state([1, 2]));
  await post(state([2, 3, 4]));             // 2 is a repeat
  const ev = await (await fetch(`${base}/api/events`)).json();
  assert.deepEqual(ev.map((e) => e.seq), [4, 3, 2, 1]);
});

test("engine restart (seq goes backwards) clears old events", async () => {
  await post(state([1]));
  const ev = await (await fetch(`${base}/api/events`)).json();
  assert.deepEqual(ev.map((e) => e.seq), [1]);
});
