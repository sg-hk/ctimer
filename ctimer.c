/*	ctimer by sg-hk
	pomodoro timer in C
	works as standalone app
	or integrated within:
	tiramisu, creek, ...	*/

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CTIMER "/.local/share/ctimer/"

void play_sound(const char *sound_file) {
	pid_t pid = fork();

	if (pid == -1) {
		perror("mpv fork error: ");
		exit(EXIT_FAILURE);
	} else if (pid == 0) {
		int devNull = open("/dev/null", O_WRONLY);
		if (devNull == -1) {
			perror("Failed to open /dev/null");
			exit(EXIT_FAILURE);
		}

		dup2(devNull, STDOUT_FILENO);

		execlp("mpv", "mpv", "--no-video", "--quiet", sound_file, (char *)NULL);
		perror("mpv exec error: ");
		exit(EXIT_FAILURE);
	} else {
		/* note that here we wait for the sound file to be played
		   the countdown_timer functions starts after it; intended behaviour
		   because break/work switches require these 1-2s       */
		int status;
		waitpid(pid, &status, 0);
	}
}

void countdown_timer(int seconds) 
{
	for (int elapsed = 0; elapsed < seconds; ++elapsed) {
		int remaining = seconds - elapsed;
		printf("\r%02u:%02u", remaining / 60, remaining % 60);
		fflush(stdout);
		sleep(1);
	}
	printf("\n");
}

void notify(char *mode, int n_sessions, int total_time, int work_time, int i, int ptime, int sbktime, int lbktime)
{
	/* sends notification to stdout
	   and plays the relevant bell */

        const char *username = getenv("USER");
	const char *home = getenv("HOME");
	if (!username || !home) {
		fprintf(stderr, "Error: USER or HOME env vars not set.\n");
		return;
	}

	char start_path[128], end_path[128], finished_path[128];
	snprintf(start_path, sizeof(start_path), "%s%s%s", home, CTIMER, "start.mp3");
	const char *start = start_path;
 	snprintf(end_path, sizeof(end_path), "%s%s%s", home, CTIMER, "end.mp3");
	const char *end = end_path;
	snprintf(finished_path, sizeof(finished_path), "%s%s%s", home, CTIMER, "finished.mp3");
	const char *finished = finished_path;

        if (strcmp(mode, "first_pomodoro") == 0) {
		printf("Ctimer has started! Good luck\n");
                printf("%d pomodoros of %d minutes, with short breaks of %d minutes and long breaks of %d minutes\n", n_sessions, ptime, sbktime, lbktime);
                printf("Total session time of %d minutes, of which %d are work\n", total_time, work_time);
                play_sound(start);
        } else if (strcmp(mode, "pomodoro") == 0) {
                printf("[%d/%d] pomodoro will last %d minutes\n", i + 1, n_sessions, ptime);
                play_sound(start);
        } else if (strcmp(mode, "short_break") == 0) {
                printf("[%d/%d] break will last %d minutes\n", i + 1, n_sessions - 1, sbktime);
                play_sound(end);
        } else if (strcmp(mode, "long_break") == 0) {
                printf("[%d/%d] break will last %d minutes\n", i + 1, n_sessions - 1, lbktime);
                play_sound(end);
        } else if (strcmp(mode, "finished") == 0) {
                play_sound(finished);
                printf("All tasks done. Bravo, %s!\n", username);
        }
}

void ex_cmd(const char *command, char *const argv[])
{
        execvp(command, argv);
        perror("Exec error: ");
        exit(EXIT_FAILURE);
}

void pomodoro_timer(int n_sessions, int ptime, int sbktime, int lbktime, int frequency)
{
        int full_n_sessions = (n_sessions - 1) / frequency;
        int work_time = ptime * n_sessions;
        int total_time = work_time + lbktime * full_n_sessions + sbktime * (n_sessions - 1 - full_n_sessions);
        int i = 0;
        char *mode = NULL;

        for (i = 0; i < n_sessions; ++i) {
                if (i == 0) {
                        mode = "first_pomodoro";
                } else {
                        mode = "pomodoro";
                }

                notify(mode, n_sessions, total_time, work_time, i, ptime, 0, 0);
		countdown_timer(ptime * 60);

                if (i < n_sessions - 1) {
                        if ((i + 1) % frequency == 0) {
                                mode = "long_break";
	                        notify(mode, n_sessions, 0, 0, i, 0, 0, lbktime);
				countdown_timer(lbktime * 60);
                        } else {
                                mode = "short_break";
	                        notify(mode, n_sessions, 0, 0, i, 0, sbktime, 0);
				countdown_timer(sbktime * 60);
                        }
                }
        }

        mode = "finished";
        notify(mode, 0, 0, 0, 0, 0, 0, 0);
}

int main(int argc, char *argv[])
{
        int n_sessions = 5;
        int ptime = 25;
        int sbktime = 5;
        int lbktime = 15;
        int frequency = 4;

        int opt;
        while ((opt = getopt(argc, argv, "n:t:s:l:f:")) != -1) {
                switch (opt) {
                case 'n':
                        n_sessions = atoi(optarg);
                        break;
                case 't':
                        ptime = atoi(optarg);
                        break;
                case 's':
                        sbktime = atoi(optarg);
                        break;
                case 'l':
                        lbktime = atoi(optarg);
                        break;
                case 'f':
                        frequency = atoi(optarg);
                        break;
                default:
                        fprintf(stderr, "Usage: %s [-n n_sessions] [-t pomodoro time] [-s short break time] [-l long break time] [-f frequency (sessions before a long break)]\n", argv[0]);
                        return 1;
                }
        }

        pomodoro_timer(n_sessions, ptime, sbktime, lbktime, frequency);

        return 0;
}
