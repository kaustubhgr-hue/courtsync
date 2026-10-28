// app.js - Express API. Keeps the latest scoreboard state and an event history in memory.
import express from "express";
import cors from "cors";

const MAX_EVENTS = 100;

// token: if set, POST /api/state must send it in the X-Token header (so strangers can't post scores).
export function createApp({ token } = {}) {
  const app = express();
  app.use(cors());                 // let the Vite page (different port) call this API
  app.use(express.json());

  let latest = null;               // last state posted by the bridge
  let events = [];                 // all events seen, oldest first (capped at MAX_EVENTS)
  let lastSeq = 0;                 // highest event seq stored

  // Bridge posts the full engine state here every second.
  app.post("/api/state", (req, res) => {
    if (token && req.get("x-token") !== token) {
      return res.status(401).json({ error: "missing or wrong X-Token" });
    }
    const s = req.body;
    if (!s || typeof s !== "object" || !Array.isArray(s.teams) || !Array.isArray(s.events)) {
      return res.status(400).json({ error: "expected engine state JSON" });
    }

    // The engine only keeps its last 10 events, so we collect new ones by seq number.
    const maxSeq = s.events.reduce((m, e) => Math.max(m, e.seq), 0);
    if (maxSeq < lastSeq) {        // engine was restarted: seq went backwards, start over
      events = [];
      lastSeq = 0;
    }
    for (const e of s.events) {
      if (e.seq > lastSeq) {
        events.push(e);
        lastSeq = e.seq;
      }
    }
    events = events.slice(-MAX_EVENTS);

    latest = { ...s, received_at: Date.now() };
    res.json({ ok: true });
  });

  app.get("/api/state", (req, res) => {
    if (!latest) return res.status(404).json({ error: "no state yet" });
    res.json(latest);
  });

  // Newest first, ready to show in a feed.
  app.get("/api/events", (req, res) => {
    res.json([...events].reverse());
  });

  return app;
}
