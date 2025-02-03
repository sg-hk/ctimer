/* ctimer v1.2 by sg-hk
   simple pomodoro timer */

#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CTIMER "/.local/share/ctimer/"

char start_msg[512], over_msg[128];
char startfp[128], endfp[128], overfp[128];

void initialize_strings
(int n, int ptime, int sbktime, int lbktime, int ttime, int wtime)
{
        /* audio paths */
        snprintf(startfp, sizeof(startfp), "%s%s%s", 
                        getenv("HOME"), CTIMER, "start.mp3");
        snprintf(endfp, sizeof(endfp), "%s%s%s", 
                        getenv("HOME"), CTIMER, "end.mp3");
        snprintf(overfp, sizeof(overfp), "%s%s%s", 
                        getenv("HOME"), CTIMER, "over.mp3");

        /* notifications */
        snprintf(start_msg, sizeof(start_msg),
                "Ctimer has started! Good luck\n"
                "%d pomodoros of %d minutes, with "
                "short breaks of %d minutes and long breaks of %d minutes\n"
                "Total session time of %d minutes, of which %d are work",
                n, ptime, sbktime, lbktime, ttime, wtime);
        snprintf(over_msg, sizeof(over_msg), 
                "All %d pomodoros done. It's over, %s\n", 
                n, getenv("USER"));
}

void play_sound (const char *filepath)
{
        pid_t pid = fork();
        if (pid == -1) {
                perror("mpv fork");
                return;
        } else if (pid == 0) {
                int dev_null = open("/dev/null", O_WRONLY);
                if (dev_null == -1) {
                        perror("cant open /dev/null");
                        return;
                }
                dup2(dev_null, STDOUT_FILENO);
                close(dev_null);

                execlp("mpv", "mpv", filepath, (char*)NULL);
                perror("mpv cmd");
                return;
        } else {
                int status;
                waitpid(pid, &status, WNOHANG);
        }
}

void herbe (char *msg)
{
        if (strlen(msg) >= 500) {
                fprintf(stderr, "string [%s] too long\n", msg);
                return;
        }
        char cmd[512];
        snprintf(cmd, sizeof(cmd), "herbe '%s'", msg);
        pid_t pid = fork(); 
        if (pid == -1) {
                perror("herbe fork");
                return;
        } else if (pid == 0) {
                execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
                perror("execl");
                return;
        } else {
                int status;
                waitpid(pid, &status, WNOHANG);
        }
}

void countdown_timer (int seconds) 
{
        for (int elapsed = 0; elapsed < seconds; ++elapsed) {
                int remaining = seconds - elapsed;
                printf("\r%02u:%02u\n", remaining / 60, remaining % 60);
                fflush(stdout);
                sleep(1);
        }
}


void log_pomodoro (int ptime, char *category)
{
        time_t now = time(NULL);
        struct tm *t = localtime(&now);
        int year = t->tm_year+1900;
        int month = t->tm_mon+1;
        int day = t->tm_mday;
        char date[64];
        snprintf(date, sizeof(date), "%04d-%02d-%02d", year, month, day);

        char path[128];
        snprintf(path, sizeof(path), "%s%s%s%s", 
                        getenv("HOME"), CTIMER, date, "_pomodoros.log");
        FILE *log = fopen(path, "a+");
        if (!log) { perror("error opening log"); return; }

        char entry[64];
        snprintf(entry, sizeof(entry), "%d|%s|%d:%d\n", 
                        ptime, category, t->tm_hour, t->tm_min);
        fputs(entry, log);
        fclose(log);
}

void pomodoro 
(int n, int ptime, int sbktime, int lbktime, int frq, char *category)
{
        for (int i = 0; i < n; ++i) {
                play_sound(startfp);
                if (i == 0) {
                        /* first pomodoro, welcome message */
                        herbe(start_msg);
                } else {
                        /* dynamic message */
                        char pom_msg[128];
                        snprintf(pom_msg, sizeof(pom_msg),
                                "[%d/%d] pomodoro will last %d minutes",
                                i + 1, n, ptime);
                        herbe(pom_msg);
                }

                countdown_timer(ptime * 60);

                log_pomodoro(ptime, category);
                /* check if end of session */
                if (i == n - 1) {
                        play_sound(overfp);
                        herbe(over_msg);
                        return;
                }

                /* otherwise it's a break */
                play_sound(endfp);
                char bk_msg[128];
                if ((i + 1) % frq == 0) {
                        snprintf(bk_msg, sizeof(bk_msg), 
                                "[%d/%d] long break will last %d minutes\n", 
                                i + 1, n - 1, lbktime);
                        herbe(bk_msg);
                        countdown_timer(lbktime * 60);
                } else {
                        snprintf(bk_msg, sizeof(bk_msg), 
                                "[%d/%d] short break will last %d minutes\n", 
                                i + 1, n - 1, sbktime);
                        herbe(bk_msg);
                        countdown_timer(sbktime * 60);
                }
        }
}

void printhelp (char *argv_0)
{
        fprintf(stderr,
                        "Usage: %s\n"
                        "   [-n number of pomodoros]\n"
                        "   [-t pomodoro time]\n"
                        "   [-T target session time]\n"
                        "   [-s short break time]\n"
                        "   [-l long break time]\n"
                        "   [-f frq (sessions before a long break)]\n"
                        "   [-c category (for logging)]\n"
                        "   [-h to print this message and exit]\n"
                        "Use man ctimer for more detailed information\n"
                        "Default: 5 pomodoros of 25 minutes, 5 then 15m breaks",
                        argv_0);
        exit(EXIT_FAILURE);
}

int main (int argc, char **argv)
{
        int n = 5;
        int ptime = 25;
        int ttime = 0;
        int sbktime = 5;
        int lbktime = 15;
        int frq = 4;
        char *category = NULL;

        int opt;
        while ((opt = getopt(argc, argv, "n:t:T:s:l:f:c:")) != -1) {
                switch (opt) {
                        case 'n':
                                n = atoi(optarg);
                                break;
                        case 't':
                                ptime = atoi(optarg);
                                break;
                        case 'T':
                                ttime = atoi(optarg);
                                break;
                        case 's':
                                sbktime = atoi(optarg);
                                break;
                        case 'l':
                                lbktime = atoi(optarg);
                                break;
                        case 'f':
                                frq = atoi(optarg);
                                break;
                        case 'c':
                                category = optarg;
                                break;
                        case 'h':
                                printhelp(argv[0]);
                                break;
                        default:
                                printhelp(argv[0]);
                }
        }

        /* derive rest of integers */
        int n_lbk = (n - 1) / frq; // -1 because no break at end
        int wtime;
        if (ttime != 0 && ptime != 25) {
                /* cannot determine custom total time AND custom pom time */
                return 1;
        } else if (ttime == 0) {
                /* custom pom time */
                wtime = ptime * n;
                ttime = wtime + lbktime * n_lbk + sbktime * (n - 1 - n_lbk);
        } else {
                /* custom total time */
                ptime = (ttime - lbktime * n_lbk - sbktime * (n - 1 - n_lbk)) 
                        / n;	
                wtime = ptime * n;
        }

        /* create strings accordingly */
        initialize_strings(n, ptime, sbktime, lbktime, ttime, wtime);

        /* start */
        pomodoro(n, ptime, sbktime, lbktime, frq, category);

        return 0;
}
