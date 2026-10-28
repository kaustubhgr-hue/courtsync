/* scoreboard.c - scoreboard rules and state machine */
#include <stdarg.h>
#include <string.h>
#include "scoreboard.h"

/* Add an event to the ring buffer (overwrites the oldest when full) and the log. */
static void add_event(Scoreboard *sb, const char *fmt, ...)
{
    Event *e = &sb->events[sb->ev_head];
    int n = snprintf(e->text, EVENT_TEXT, "Q%d %02d:%02d ",
                     sb->quarter, sb->clock / 60, sb->clock % 60);
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(e->text + n, EVENT_TEXT - n, fmt, ap);
    va_end(ap);

    e->seq = ++sb->ev_seq;
    sb->ev_head = (sb->ev_head + 1) % EVENT_RING;
    if (sb->ev_count < EVENT_RING)
        sb->ev_count++;

    if (sb->log) {
        fprintf(sb->log, "[%u] %s\n", e->seq, e->text);
        fflush(sb->log);
    }
}

void sb_init(Scoreboard *sb)
{
    memset(sb, 0, sizeof *sb);
    sb->state = PAUSED;
    sb->quarter = 1;
    sb->clock = QUARTER_SECS;
    sb->shot_clock = SHOT_CLOCK_SECS;
    strcpy(sb->teams[HOME].name, "HOME");
    strcpy(sb->teams[AWAY].name, "AWAY");
    for (int t = 0; t < NUM_TEAMS; t++)
        sb->teams[t].timeouts = TIMEOUTS_PER_TEAM;
}

const char *sb_start(Scoreboard *sb)
{
    if (sb->state == ENDED)   return "game has ended";
    if (sb->state == RUNNING) return "already running";
    if (sb->flags & FLAG_QUARTER_OVER) return "quarter is over, use next_quarter";
    sb->flags &= ~FLAG_SHOT_VIOLATION;
    sb->state = RUNNING;
    add_event(sb, "Clock started");
    return NULL;
}

const char *sb_pause(Scoreboard *sb)
{
    if (sb->state != RUNNING) return "clock is not running";
    sb->state = PAUSED;
    add_event(sb, "Clock paused");
    return NULL;
}

const char *sb_score(Scoreboard *sb, int team, int pts)
{
    if (sb->state == ENDED)    return "game has ended";
    if (pts < 1 || pts > 3)    return "points must be 1, 2 or 3";
    sb->teams[team].score += pts;
    sb->shot_clock = SHOT_CLOCK_SECS;      /* possession changes after a score */
    add_event(sb, "%s scores %d (%d-%d)", sb->teams[team].name, pts,
              sb->teams[HOME].score, sb->teams[AWAY].score);
    return NULL;
}

const char *sb_foul(Scoreboard *sb, int team, int player)
{
    if (sb->state == ENDED)                         return "game has ended";
    if (player < 1 || player > PLAYERS_PER_TEAM)    return "player must be 1-5";
    Team *t = &sb->teams[team];
    int pf = ++t->player_fouls[player - 1];
    t->fouls++;
    add_event(sb, "Foul %s #%d (player %d, team %d)", t->name, player, pf, t->fouls);

    if (pf == FOUL_OUT)
        add_event(sb, "%s #%d fouled out", t->name, player);
    if (t->fouls >= PENALTY_FOULS && !(sb->flags & (FLAG_PENALTY_HOME << team))) {
        sb->flags |= FLAG_PENALTY_HOME << team;
        add_event(sb, "%s in penalty", t->name);
    }
    return NULL;
}

const char *sb_timeout(Scoreboard *sb, int team)
{
    if (sb->state == ENDED)   return "game has ended";
    if (sb->state == TIMEOUT) return "timeout already in progress";
    if (sb->teams[team].timeouts == 0) return "no timeouts left";
    sb->teams[team].timeouts--;
    sb->state = TIMEOUT;                   /* clock stops until 'start' */
    add_event(sb, "Timeout %s (%d left)", sb->teams[team].name, sb->teams[team].timeouts);
    return NULL;
}

const char *sb_next_quarter(Scoreboard *sb)
{
    if (sb->state == ENDED) return "game has ended";
    if (sb->state == RUNNING) return "pause the clock first";
    if (sb->quarter >= NUM_QUARTERS) {
        sb->state = ENDED;
        add_event(sb, "Game over %d-%d", sb->teams[HOME].score, sb->teams[AWAY].score);
        return NULL;
    }
    sb->quarter++;
    sb->state = PAUSED;
    sb->clock = QUARTER_SECS;
    sb->shot_clock = SHOT_CLOCK_SECS;
    sb->flags = 0;                         /* penalty and quarter flags reset */
    for (int t = 0; t < NUM_TEAMS; t++)
        sb->teams[t].fouls = 0;
    add_event(sb, "Start of quarter %d", sb->quarter);
    return NULL;
}

/* One loop pass = one simulated second. Stops early on a quarter end or violation. */
const char *sb_tick(Scoreboard *sb, int secs)
{
    if (secs < 1) return "ticks must be >= 1";
    for (int i = 0; i < secs && sb->state == RUNNING; i++) {
        sb->clock--;
        sb->shot_clock--;
        if (sb->clock == 0) {
            sb->state = PAUSED;
            sb->flags |= FLAG_QUARTER_OVER;
            add_event(sb, "End of quarter %d", sb->quarter);
        } else if (sb->shot_clock == 0) {
            sb->state = PAUSED;
            sb->flags |= FLAG_SHOT_VIOLATION;
            sb->shot_clock = SHOT_CLOCK_SECS;
            add_event(sb, "Shot clock violation");
        }
    }
    return NULL;
}

const char *sb_state_name(GameState s)
{
    static const char *names[] = { "PAUSED", "RUNNING", "TIMEOUT", "ENDED" };
    return names[s];
}

const Event *sb_nth_event(const Scoreboard *sb, int i)
{
    if (i < 0 || i >= sb->ev_count) return NULL;
    int oldest = (sb->ev_head - sb->ev_count + EVENT_RING) % EVENT_RING;
    return &sb->events[(oldest + i) % EVENT_RING];
}

/* Build the state as one line of JSON. Event text never contains quotes. */
int sb_to_json(const Scoreboard *sb, char *buf, int size)
{
    int n = 0;
#define PUT(...) do { if (n < size) n += snprintf(buf + n, size - n, __VA_ARGS__); } while (0)
    PUT("{\"state\":\"%s\",\"quarter\":%d,\"clock\":%d,\"shot_clock\":%d,\"flags\":%u,\"teams\":[",
        sb_state_name(sb->state), sb->quarter, sb->clock, sb->shot_clock, sb->flags);
    for (int t = 0; t < NUM_TEAMS; t++) {
        const Team *tm = &sb->teams[t];
        PUT("%s{\"name\":\"%s\",\"score\":%d,\"fouls\":%d,\"timeouts\":%d,\"player_fouls\":[",
            t ? "," : "", tm->name, tm->score, tm->fouls, tm->timeouts);
        for (int p = 0; p < PLAYERS_PER_TEAM; p++)
            PUT("%s%d", p ? "," : "", tm->player_fouls[p]);
        PUT("]}");
    }
    PUT("],\"events\":[");
    for (int i = 0; i < sb->ev_count; i++) {
        const Event *e = sb_nth_event(sb, i);
        PUT("%s{\"seq\":%u,\"text\":\"%s\"}", i ? "," : "", e->seq, e->text);
    }
    PUT("]}");
#undef PUT
    return n < size ? n : size - 1;
}

void sb_box_score(const Scoreboard *sb, FILE *out)
{
    fprintf(out, "\n=========== BOX SCORE ===========\n");
    fprintf(out, "Final: %s %d - %d %s   (Q%d, %s)\n",
            sb->teams[HOME].name, sb->teams[HOME].score,
            sb->teams[AWAY].score, sb->teams[AWAY].name,
            sb->quarter, sb_state_name(sb->state));
    for (int t = 0; t < NUM_TEAMS; t++) {
        const Team *tm = &sb->teams[t];
        fprintf(out, "\n%s: score %d, quarter fouls %d, timeouts left %d\n",
                tm->name, tm->score, tm->fouls, tm->timeouts);
        fprintf(out, "  player fouls:");
        for (int p = 0; p < PLAYERS_PER_TEAM; p++)
            fprintf(out, " #%d=%d", p + 1, tm->player_fouls[p]);
        fprintf(out, "\n");
    }
    fprintf(out, "=================================\n");
}
