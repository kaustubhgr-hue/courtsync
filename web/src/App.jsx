// App.jsx - polls the API every second and shows the scoreboard and event feed.
import { useEffect, useState } from "react";

// In development (Vite on :5173) the API is on port 3000 of the same machine.
// In production the Express server serves this page itself, so the API is same-origin.
const API = import.meta.env.DEV ? `http://${window.location.hostname}:3000` : "";

// Same bit flags as engine/include/scoreboard.h
const FLAG_SHOT_VIOLATION = 1 << 2;
const FLAG_QUARTER_OVER = 1 << 3;

const mmss = (secs) =>
  `${String(Math.floor(secs / 60)).padStart(2, "0")}:${String(secs % 60).padStart(2, "0")}`;

function Team({ team, inPenalty }) {
  return (
    <div className="team">
      <div className="team-name">{team.name}</div>
      <div className="score">{team.score}</div>
      <div className="team-stats">
        <span>Team fouls <b>{team.fouls}</b></span>
        <span>Timeouts <b>{team.timeouts}</b></span>
      </div>
      {inPenalty && <div className="tag">PENALTY</div>}
    </div>
  );
}

export default function App() {
  const [game, setGame] = useState(null);
  const [events, setEvents] = useState([]);
  const [conn, setConn] = useState("connecting");   // connecting | live | waiting | offline

  useEffect(() => {
    async function poll() {
      try {
        const [stateRes, eventsRes] = await Promise.all([
          fetch(`${API}/api/state`),
          fetch(`${API}/api/events`),
        ]);
        setEvents(await eventsRes.json());
        if (stateRes.status === 404) {
          setConn("waiting");                         // server up, bridge hasn't posted yet
          return;
        }
        setGame(await stateRes.json());
        setConn("live");
      } catch {
        setConn("offline");                           // server not reachable
      }
    }
    poll();
    const id = setInterval(poll, 1000);
    return () => clearInterval(id);                   // stop polling if the page unmounts
  }, []);

  return (
    <main>
      <header>
        <h1>CourtSync</h1>
        <span className={`conn conn-${conn}`}>{conn}</span>
      </header>

      {!game ? (
        <p className="empty">
          {conn === "offline"
            ? `Can't reach the server${API ? ` at ${API}` : ""}. Is "npm run server" running?`
            : "Waiting for the first state from the bridge..."}
        </p>
      ) : (
        <>
          <section className="board">
            <Team team={game.teams[0]} inPenalty={(game.flags & 1) !== 0} />
            <div className="center">
              <div className="quarter">{game.state === "ENDED" ? "FINAL" : `Q${game.quarter}`}</div>
              <div className="clock">{mmss(game.clock)}</div>
              <div className="shot">
                <span>SHOT</span>
                <b className={game.shot_clock <= 5 ? "low" : ""}>{game.shot_clock}</b>
              </div>
              <div className={`state state-${game.state}`}>{game.state}</div>
              {(game.flags & FLAG_SHOT_VIOLATION) !== 0 && <div className="tag">SHOT CLOCK VIOLATION</div>}
              {(game.flags & FLAG_QUARTER_OVER) !== 0 && <div className="tag">END OF QUARTER</div>}
            </div>
            <Team team={game.teams[1]} inPenalty={(game.flags & 2) !== 0} />
          </section>

          <section className="events">
            <h2>Events</h2>
            {events.length === 0 ? (
              <p className="empty">No events yet.</p>
            ) : (
              <ul>
                {events.map((e) => (
                  <li key={e.seq}>
                    <span className="seq">#{e.seq}</span> {e.text}
                  </li>
                ))}
              </ul>
            )}
          </section>
        </>
      )}
    </main>
  );
}
