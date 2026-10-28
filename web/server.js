// server.js - starts the API on port 3000 (or $PORT), on all interfaces so a VM can reach it.
import { createApp } from "./app.js";

const PORT = process.env.PORT || 3000;
createApp().listen(PORT, () => {
  console.log(`CourtSync API on http://localhost:${PORT}`);
});
