/* scoreboard.h - CourtSync basketball scoreboard engine (core logic, no I/O except log/JSON) */
#ifndef SCOREBOARD_H
#define SCOREBOARD_H

#include <stdio.h>

#define NUM_TEAMS         2
#define HOME              0
#define AWAY              1
#define PLAYERS_PER_TEAM  5
#define NUM_QUARTERS      4
#define QUARTER_SECS      600   /* 10 minute quarters */
#define SHOT_CLOCK_SECS   24
#define TIMEOUTS_PER_TEAM 5
#define FOUL_OUT          6     /* player fouls out at 6 fouls */
#define PENALTY_FOULS     5     /* team fouls per quarter before penalty */
#define EVENT_RING        10    /* recent events kept in memory */
#define EVENT_TEXT        64

/* Game state machine */
typedef enum { PAUSED, RUNNING, TIMEOUT, ENDED } GameState;

/* Status bit flags (sb->flags) */
#define FLAG_PENALTY_HOME   (1u << 0)  /* home has 5+ team fouls this quarter */
#define FLAG_PENALTY_AWAY   (1u << 1)  /* away has 5+ team fouls this quarter */
#define FLAG_SHOT_VIOLATION (1u << 2)  /* shot clock ran out (cleared on start) */
#define FLAG_QUARTER_OVER   (1u << 3)  /* game clock hit 0 in this quarter */

typedef struct {
    unsigned seq;              /* increasing event number */
    char text[EVENT_TEXT];
} Event;

typedef struct {
    char name[8];
    int score;
    int fouls;                 /* team fouls this quarter */
    int timeouts;              /* timeouts left */
    int player_fouls[PLAYERS_PER_TEAM];
} Team;

typedef struct {
    GameState state;
    int quarter;               /* 1..NUM_QUARTERS */
    int clock;                 /* game clock, seconds left in quarter */
    int shot_clock;            /* seconds left */
    unsigned flags;
    Team teams[NUM_TEAMS];
    Event events[EVENT_RING];  /* ring buffer of recent events */
    int ev_head;               /* next slot to write */
    int ev_count;              /* how many slots are filled */
    unsigned ev_seq;
    FILE *log;                 /* optional event log (NULL = no logging) */
} Scoreboard;

/* Commands return NULL on success, or an error message string. */
void        sb_init(Scoreboard *sb);
const char *sb_start(Scoreboard *sb);
const char *sb_pause(Scoreboard *sb);
const char *sb_score(Scoreboard *sb, int team, int pts);
const char *sb_foul(Scoreboard *sb, int team, int player);   /* player is 1-based */
const char *sb_timeout(Scoreboard *sb, int team);
const char *sb_next_quarter(Scoreboard *sb);
const char *sb_tick(Scoreboard *sb, int secs);               /* advance simulated time */

const char  *sb_state_name(GameState s);
const Event *sb_nth_event(const Scoreboard *sb, int i);      /* i=0 is oldest */
int          sb_to_json(const Scoreboard *sb, char *buf, int size);  /* one line, no newline */
void         sb_box_score(const Scoreboard *sb, FILE *out);

#endif
