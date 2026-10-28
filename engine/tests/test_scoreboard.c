/* test_scoreboard.c - tiny test runner, no framework. Exit code 1 if any check fails. */
#include <string.h>
#include "scoreboard.h"

static int checks = 0, failures = 0;

#define CHECK(cond) do { checks++; if (!(cond)) { failures++; \
    printf("FAIL line %d: %s\n", __LINE__, #cond); } } while (0)

static void test_init(void)
{
    Scoreboard sb; sb_init(&sb);
    CHECK(sb.state == PAUSED && sb.quarter == 1);
    CHECK(sb.clock == QUARTER_SECS && sb.shot_clock == SHOT_CLOCK_SECS);
    CHECK(sb.teams[HOME].timeouts == TIMEOUTS_PER_TEAM);
}

static void test_clock(void)
{
    Scoreboard sb; sb_init(&sb);
    sb_tick(&sb, 5);                          /* paused: nothing moves */
    CHECK(sb.clock == QUARTER_SECS);
    CHECK(sb_start(&sb) == NULL && sb.state == RUNNING);
    CHECK(sb_start(&sb) != NULL);             /* already running */
    sb_tick(&sb, 10);
    CHECK(sb.clock == QUARTER_SECS - 10 && sb.shot_clock == SHOT_CLOCK_SECS - 10);
    CHECK(sb_pause(&sb) == NULL && sb.state == PAUSED);
}

static void test_shot_clock(void)
{
    Scoreboard sb; sb_init(&sb);
    sb_start(&sb);
    sb_tick(&sb, 30);                         /* stops at 24 with a violation */
    CHECK(sb.state == PAUSED && (sb.flags & FLAG_SHOT_VIOLATION));
    CHECK(sb.clock == QUARTER_SECS - 24 && sb.shot_clock == SHOT_CLOCK_SECS);
    sb_start(&sb);
    CHECK(!(sb.flags & FLAG_SHOT_VIOLATION));
    sb_tick(&sb, 10);
    sb_score(&sb, HOME, 2);                   /* a score resets the shot clock */
    CHECK(sb.shot_clock == SHOT_CLOCK_SECS);
}

static void test_score(void)
{
    Scoreboard sb; sb_init(&sb);
    CHECK(sb_score(&sb, HOME, 3) == NULL && sb_score(&sb, AWAY, 2) == NULL);
    CHECK(sb.teams[HOME].score == 3 && sb.teams[AWAY].score == 2);
    CHECK(sb_score(&sb, HOME, 4) != NULL);    /* invalid points rejected */
    CHECK(sb.teams[HOME].score == 3);
}

static void test_fouls(void)
{
    Scoreboard sb; sb_init(&sb);
    CHECK(sb_foul(&sb, AWAY, 0) != NULL && sb_foul(&sb, AWAY, 6) != NULL);
    for (int i = 0; i < 4; i++) sb_foul(&sb, AWAY, 1 + i);
    CHECK(sb.teams[AWAY].fouls == 4 && !(sb.flags & FLAG_PENALTY_AWAY));
    sb_foul(&sb, AWAY, 5);
    CHECK(sb.teams[AWAY].fouls == 5 && (sb.flags & FLAG_PENALTY_AWAY));
    CHECK(!(sb.flags & FLAG_PENALTY_HOME));
    CHECK(sb.teams[AWAY].player_fouls[0] == 1);
    sb_next_quarter(&sb);                     /* team fouls and penalty reset */
    CHECK(sb.teams[AWAY].fouls == 0 && sb.flags == 0);
    CHECK(sb.teams[AWAY].player_fouls[0] == 1);   /* player fouls carry over */
}

static void test_timeouts(void)
{
    Scoreboard sb; sb_init(&sb);
    sb_start(&sb);
    CHECK(sb_timeout(&sb, HOME) == NULL && sb.state == TIMEOUT);
    CHECK(sb.teams[HOME].timeouts == TIMEOUTS_PER_TEAM - 1);
    CHECK(sb_timeout(&sb, AWAY) != NULL);     /* one at a time */
    sb_tick(&sb, 5);
    CHECK(sb.clock == QUARTER_SECS);          /* clock frozen */
    sb_start(&sb);
    CHECK(sb.state == RUNNING);
    sb.teams[AWAY].timeouts = 0;
    sb_pause(&sb);
    CHECK(sb_timeout(&sb, AWAY) != NULL);     /* none left */
}

static void test_quarters(void)
{
    Scoreboard sb; sb_init(&sb);
    sb_start(&sb);
    sb_tick(&sb, QUARTER_SECS);               /* shot clock violations pause us; keep going */
    while (!(sb.flags & FLAG_QUARTER_OVER)) { sb_start(&sb); sb_tick(&sb, QUARTER_SECS); }
    CHECK(sb.clock == 0 && sb.state == PAUSED);
    CHECK(sb_start(&sb) != NULL);             /* must advance first */
    for (int q = 2; q <= NUM_QUARTERS; q++) {
        CHECK(sb_next_quarter(&sb) == NULL && sb.quarter == q);
        CHECK(sb.clock == QUARTER_SECS && sb.state == PAUSED);
    }
    CHECK(sb_next_quarter(&sb) == NULL && sb.state == ENDED);
    CHECK(sb_start(&sb) != NULL && sb_score(&sb, HOME, 2) != NULL);
}

static void test_ring_buffer(void)
{
    Scoreboard sb; sb_init(&sb);
    for (int i = 0; i < EVENT_RING + 5; i++) sb_score(&sb, HOME, 1);
    CHECK(sb.ev_count == EVENT_RING);
    CHECK(sb_nth_event(&sb, 0)->seq == 6);                 /* oldest kept */
    CHECK(sb_nth_event(&sb, EVENT_RING - 1)->seq == EVENT_RING + 5);
    CHECK(sb_nth_event(&sb, EVENT_RING) == NULL);
}

static void test_json(void)
{
    Scoreboard sb; sb_init(&sb);
    sb_score(&sb, HOME, 3);
    char buf[4096];
    int n = sb_to_json(&sb, buf, sizeof buf);
    CHECK(n > 0 && n == (int)strlen(buf));
    CHECK(strstr(buf, "\"state\":\"PAUSED\"") != NULL);
    CHECK(strstr(buf, "\"score\":3") != NULL);
    CHECK(strstr(buf, "HOME scores 3") != NULL);
    CHECK(strchr(buf, '\n') == NULL);
    char small[20];                           /* truncation must not overflow */
    CHECK(sb_to_json(&sb, small, sizeof small) < (int)sizeof small);
}

int main(void)
{
    test_init(); test_clock(); test_shot_clock(); test_score(); test_fouls();
    test_timeouts(); test_quarters(); test_ring_buffer(); test_json();
    printf("%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
