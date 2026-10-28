/* main.c - terminal front end: reads commands, writes state.json (and optional device) */
#include <stdlib.h>
#include <string.h>
#include "scoreboard.h"

static const char *state_path = "state.json";
static const char *log_path   = "events.log";
static const char *device_path = NULL;   /* set by --device */

/* Write the current state to state.json, and to the device file if requested. */
static void publish(const Scoreboard *sb)
{
    char json[4096];
    sb_to_json(sb, json, sizeof json);

    FILE *f = fopen(state_path, "w");
    if (f) { fprintf(f, "%s\n", json); fclose(f); }

    if (device_path) {
        f = fopen(device_path, "w");
        if (f) { fprintf(f, "%s\n", json); fclose(f); }
        else   fprintf(stderr, "warning: cannot open device %s\n", device_path);
    }
}

static void print_status(const Scoreboard *sb)
{
    printf("Q%d %s  clock %02d:%02d  shot %02d | %s %d - %d %s | team fouls %d-%d | timeouts %d-%d | flags 0x%X\n",
           sb->quarter, sb_state_name(sb->state), sb->clock / 60, sb->clock % 60,
           sb->shot_clock, sb->teams[HOME].name, sb->teams[HOME].score,
           sb->teams[AWAY].score, sb->teams[AWAY].name,
           sb->teams[HOME].fouls, sb->teams[AWAY].fouls,
           sb->teams[HOME].timeouts, sb->teams[AWAY].timeouts, sb->flags);
    for (int i = 0; i < sb->ev_count; i++)
        printf("  [%u] %s\n", sb_nth_event(sb, i)->seq, sb_nth_event(sb, i)->text);
}

/* "home"/"away" -> 0/1, anything else -> -1 */
static int parse_team(const char *s)
{
    if (strcmp(s, "home") == 0) return HOME;
    if (strcmp(s, "away") == 0) return AWAY;
    return -1;
}

int main(int argc, char **argv)
{
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--device") == 0 && i + 1 < argc) device_path = argv[++i];
        else if (strcmp(argv[i], "--state") == 0 && i + 1 < argc) state_path = argv[++i];
        else if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) log_path = argv[++i];
        else { fprintf(stderr, "usage: %s [--device path] [--state path] [--log path]\n", argv[0]); return 1; }
    }

    Scoreboard sb;
    sb_init(&sb);
    sb.log = fopen(log_path, "a");
    publish(&sb);

    printf("CourtSync engine. Commands: start, pause, tick [n], score <home|away> <pts>,\n"
           "foul <home|away> <player 1-5>, timeout <home|away>, next_quarter, status, quit\n");

    char line[128];
    while (printf("> "), fflush(stdout), fgets(line, sizeof line, stdin)) {
        char cmd[16] = "", a1[16] = "", a2[16] = "";
        if (sscanf(line, "%15s %15s %15s", cmd, a1, a2) < 1) continue;

        const char *err = NULL;
        int team = parse_team(a1);

        if (strcmp(cmd, "quit") == 0) break;
        else if (strcmp(cmd, "start") == 0)        err = sb_start(&sb);
        else if (strcmp(cmd, "pause") == 0)        err = sb_pause(&sb);
        else if (strcmp(cmd, "next_quarter") == 0) err = sb_next_quarter(&sb);
        else if (strcmp(cmd, "tick") == 0)         err = sb_tick(&sb, a1[0] ? atoi(a1) : 1);
        else if (strcmp(cmd, "status") == 0)       print_status(&sb);
        else if (strcmp(cmd, "score") == 0)        err = team < 0 ? "usage: score <home|away> <pts>" : sb_score(&sb, team, atoi(a2));
        else if (strcmp(cmd, "foul") == 0)         err = team < 0 ? "usage: foul <home|away> <player>" : sb_foul(&sb, team, atoi(a2));
        else if (strcmp(cmd, "timeout") == 0)      err = team < 0 ? "usage: timeout <home|away>" : sb_timeout(&sb, team);
        else err = "unknown command";

        if (err) printf("error: %s\n", err);
        publish(&sb);                      /* state.json is rewritten after every command */
    }

    sb_box_score(&sb, stdout);
    if (sb.log) fclose(sb.log);
    return 0;
}
