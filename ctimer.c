/* ctimer v1.4 by sg-hk
   simple pomodoro timer */

#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* constant directory paths */
static const char CTIMER[]    = "/.local/share/ctimer/";
static const char PIPE_PATH[] = "/tmp/bar/fifo_ctimer";

typedef struct {
        int  fifo_flag;
        char start_msg[512];
        char over_msg[128];
        char startfp[128];
        char endfp[128];
        char overfp[128];
} ctimer_state;

/* helper to get home directory */
static char *
get_home(void)
{
        char *home = getenv("HOME");
        if (!home) {
                fprintf(stderr, "error: home not set\n");
                exit(EXIT_FAILURE);
        }
        return home;
}

/* notification using herbe (if "make ENABLE_HERBE=1") or notify-send */
#ifdef HERBE 
static void 
notify(const char *msg)
{
        pid_t pid = fork();
        if (pid == -1) {
                perror("herbe fork");
                return;
        } else if (pid == 0) {
                char cmd[512];
                snprintf(cmd, sizeof(cmd), "herbe '%s'", msg);
                execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
                perror("execl herbe");
                exit(EXIT_FAILURE);
        } else {
                int status;
                waitpid(pid, &status, WNOHANG);
        }
}
#else
static void 
notify(const char *msg)
{
        pid_t pid = fork();
        if (pid == -1) {
                perror("notify-send fork");
                return;
        } else if (pid == 0) {
                char cmd[512];
                snprintf(cmd, sizeof(cmd),
                                "notify-send 'ctimer' '%s'", msg);
                execl("/bin/sh", "sh", "-c", cmd, (char *)NULL);
                perror("execl notify-send");
                exit(EXIT_FAILURE);
        } else {
                int status;
                waitpid(pid, &status, WNOHANG);
        }
}
#endif

/* play sound using mpv */
static void 
play_sound(const char *filepath)
{
        pid_t pid = fork();
        if (pid == -1) {
                perror("mpv fork");
                return;
        } else if (pid == 0) {
                int dev_null = open("/dev/null", O_WRONLY);
                if (dev_null == -1) {
                        perror("open /dev/null");
                        exit(EXIT_FAILURE);
                }
                dup2(dev_null, STDOUT_FILENO);
                close(dev_null);
                execlp("mpv", "mpv", filepath, (char *)NULL);
                perror("execl mpv");
                exit(EXIT_FAILURE);
        } else {
                int status;
                waitpid(pid, &status, WNOHANG);
        }
}

/* initialize runtime strings in state */
static void 
initialize_strings(ctimer_state *state, 
                   int n, int ptime, int sbktime, int lbktime, 
                   int total_time, int work_time)
{
        char *home = get_home();
        snprintf(state->startfp, sizeof(state->startfp), "%s%s%s",
                        home, CTIMER, "start.mp3");
        snprintf(state->endfp, sizeof(state->endfp), "%s%s%s",
                        home, CTIMER, "end.mp3");
        snprintf(state->overfp, sizeof(state->overfp), "%s%s%s",
                        home, CTIMER, "over.mp3");
        snprintf(state->start_msg, sizeof(state->start_msg),
                        "ctimer v1.4 started! good luck\n"
                        "%d pomodoros, each: %d m" 
                        "%d m short breaks; %d m long break\n"
                        "total: %d m; work: %d m",
                        n, ptime, sbktime, lbktime, total_time, work_time);
        snprintf(state->over_msg, sizeof(state->over_msg),
                        "all %d pomodoros done. bravo %s!", n, getenv("USER"));
}

/* countdown timer with optional fifo output */
static void 
countdown_timer(int seconds, ctimer_state *state)
{
        int fifo_fd = -1;
        if (state->fifo_flag) {
                struct stat st;
                if (stat(PIPE_PATH, &st) == -1) {
                        if (mkfifo(PIPE_PATH, 0666) == -1) {
                                perror("mkfifo");
                                exit(EXIT_FAILURE);
                        }
                }
                fifo_fd = open(PIPE_PATH, O_WRONLY);
                if (fifo_fd == -1)
                        perror("open fifo");
        }

        for (int elapsed = 0; elapsed < seconds; ++elapsed) {
                int remaining = seconds - elapsed;
                if (state->fifo_flag && fifo_fd != -1) {
                        dprintf(fifo_fd, "%02u:%02u\n", remaining / 60,
                                        remaining % 60);
                } else {
                        printf("\r%02u:%02u", remaining / 60,
                                        remaining % 60);
                        fflush(stdout);
                }
                sleep(1);
        }

        if (fifo_fd != -1) {
                close(fifo_fd);
        }
        printf("\n");
}

/* execute pomodoro cycles */
static void 
pomodoro(int n, int ptime, int sbktime, int lbktime, int frq, 
         ctimer_state *state)
{
        for (int i = 0; i < n; ++i) {
                play_sound(state->startfp);
                if (i == 0)
                        notify(state->start_msg);

                char pom_msg[128];
                snprintf(pom_msg, sizeof(pom_msg),
                                "[%d/%d] pomodoro: %d m", i + 1, n, ptime);
                countdown_timer(ptime * 60, state);
                if (i == n - 1) {
                        play_sound(state->overfp);
                        notify(state->over_msg);
                        return;
                }
                play_sound(state->endfp);
                {
                        char bk_msg[128];
                        if ((i + 1) % frq == 0) {
                                snprintf(bk_msg, sizeof(bk_msg),
                                                "[%d/%d] long break: %d m",
                                                i + 1, n - 1, lbktime);
                                notify(bk_msg);
                                countdown_timer(lbktime * 60, state);
                        } else {
                                snprintf(bk_msg, sizeof(bk_msg),
                                                "[%d/%d] short break: %d m",
                                                i + 1, n - 1, sbktime);
                                notify(bk_msg);
                                countdown_timer(sbktime * 60, state);
                        }
                }
        }
}

static void 
printhelp(const char *progname)
{
        fprintf(stderr,
                        "usage: %s\n"
                        "   [-n number of pomodoros]\n"
                        "   [-t pomodoro time in minutes]\n"
                        "   [-s short break time in minutes]\n"
                        "   [-l long break time in minutes]\n"
                        "   [-f frequency for long break]\n"
                        "   [-F output countdown to pipe]\n"
                        "   [-h print this help]\n"
                        "ctimer v1.4\n",
                        progname);
        exit(EXIT_FAILURE);
}

int 
main(int argc, char *argv[])
{
        int n = 5;
        int ptime = 25;
        int sbktime = 5;
        int lbktime = 15;
        int frq = 4;
        ctimer_state state;
        state.fifo_flag = 0;
        int opt;

        while ((opt = getopt(argc, argv, "n:t:s:l:f:Fh")) != -1) {
                switch (opt) {
                        case 'n':
                                n = atoi(optarg);
                                if (n <= 0) {
                                        fprintf(stderr,
                                                        "invalid number of pomodoros\n");
                                        exit(EXIT_FAILURE);
                                }
                                break;
                        case 't':
                                ptime = atoi(optarg);
                                if (ptime <= 0) {
                                        fprintf(stderr,
                                                        "invalid pomodoro time\n");
                                        exit(EXIT_FAILURE);
                                }
                                break;
                        case 's':
                                sbktime = atoi(optarg);
                                if (sbktime <= 0) {
                                        fprintf(stderr,
                                                        "invalid short break time\n");
                                        exit(EXIT_FAILURE);
                                }
                                break;
                        case 'l':
                                lbktime = atoi(optarg);
                                if (lbktime <= 0) {
                                        fprintf(stderr,
                                                        "invalid long break time\n");
                                        exit(EXIT_FAILURE);
                                }
                                break;
                        case 'f':
                                frq = atoi(optarg);
                                if (frq <= 0) {
                                        fprintf(stderr, "invalid frequency\n");
                                        exit(EXIT_FAILURE);
                                }
                                break;
                        case 'F':
                                state.fifo_flag = 1;
                                break;
                        case 'h':
                        default:
                                printhelp(argv[0]);
                }
        }

        int work_time    = n * ptime;
        int breaks       = n - 1;
        int long_breaks  = (frq > 0) ? breaks / frq : 0;
        int short_breaks = breaks - long_breaks;
        int total_time   = work_time + short_breaks * sbktime +
                           long_breaks * lbktime;

        initialize_strings(&state, n, ptime, sbktime, lbktime,
                           total_time, work_time);
        pomodoro(n, ptime, sbktime, lbktime, frq, &state);
        return 0;
}
