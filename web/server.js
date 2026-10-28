// server.js - starts the API on port 3000 (or $PORT), on all interfaces so a VM can reach it.
// Env vars (all optional):
//   POST_TOKEN  require this X-Token on POST /api/state
//   DEMO=1      run the C engine with a simulated game and feed it in (for the hosted demo)
import { existsSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import express from "express";
import { createApp } from "./app.js";
import { startDemo } from "./demo.js";

const PORT = process.env.PORT || 3000;
const TOKEN = process.env.POST_TOKEN || undefined;

const app = createApp({ token: TOKEN });

// After `npm run build`, also serve the dashboard itself, so one server hosts everything.
const dist = path.join(path.dirname(fileURLToPath(import.meta.url)), "dist");
if (existsSync(dist)) app.use(express.static(dist));

app.listen(PORT, (err) => {
  if (err) {                         // Express 5 passes listen errors here instead of crashing
    console.error(`cannot listen on port ${PORT}: ${err.message}`);
    process.exit(1);
  }
  console.log(`CourtSync API on http://localhost:${PORT}` +
              (existsSync(dist) ? " (serving dashboard from dist/)" : "") +
              (TOKEN ? " (POST requires X-Token)" : ""));
  if (process.env.DEMO === "1") startDemo(PORT, TOKEN);
});
