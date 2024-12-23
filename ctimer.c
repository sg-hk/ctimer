/* ctimer v1.1 by sg-hk
   simple pomodoro timer with logs */

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

typedef struct LogData {
	int minutes;
	char category[64];
	long timestamp;
} LogData;

typedef struct CatTimePair {
	char *category;
	int minutes;
} CatTimePair;

void play_sound
(const char *sound_file) 
{
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

		// redirect mpv stdout to /dev/null
		dup2(devNull, STDOUT_FILENO);
		close(devNull);

		execlp("mpv", "mpv", "--no-video", "--quiet", sound_file, (char *)NULL);
		perror("mpv exec error: ");
		exit(EXIT_FAILURE);
	} else {
		int status;
		waitpid(pid, &status, WNOHANG);
	}
}

void countdown_timer
(int seconds) 
{
	for (int elapsed = 0; elapsed < seconds; ++elapsed) {
		int remaining = seconds - elapsed;
		printf("\r%02u:%02u", remaining / 60, remaining % 60);
		fflush(stdout);
		sleep(1);
	}
	printf("\n");
}

void notify
(char *mode, int n_sessions, int total_time, int work_time, 
 int i, int ptime, int sbktime, int lbktime)
{
	const char *username = getenv("USER");
	const char *home = getenv("HOME");
	if (!username) {
		fprintf(stderr, "Error: USER env var not set\n");
		return;
	}
	if (!home) {
		fprintf(stderr, "Error: HOME not set\n");
		return;
	}

	char start[128], end[128], finished[128];
	snprintf(start, sizeof(start), "%s%s%s", home, CTIMER, "start.mp3");
	snprintf(end, sizeof(end), "%s%s%s", home, CTIMER, "end.mp3");
	snprintf(finished, sizeof(finished), "%s%s%s", home, CTIMER, "finished.mp3");

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

void log_pomodoro
(int ptime, char *category)
{
	time_t today = time(NULL);

	char path[128];
	snprintf(path, sizeof(path), "%s%s%s", getenv("HOME"), CTIMER, "pomodoros.log");
	char entry[64];
	snprintf(entry, sizeof(entry), "%d|%s|%ld\n", ptime, category, today);

	FILE *log = fopen(path, "a+");
	fputs(entry, log);

	if (fclose(log) != 0) {
		perror("Failed to close the log file");
		exit(EXIT_FAILURE);
	}
}

void pomodoro_timer
(int n_sessions, int ptime, int sbktime, int lbktime, 
 int frequency, char *category, int ttime)
{
	int i = 0, work_time = 0;
	char *mode = NULL;

	int full_n_sessions = (n_sessions - 1) / frequency;

	if (ttime == 0) {
		work_time = ptime * n_sessions;
		ttime = work_time + lbktime * full_n_sessions + sbktime * (n_sessions - 1 - full_n_sessions);
	} else {
		ptime = (ttime - lbktime * full_n_sessions - sbktime * (n_sessions - 1 - full_n_sessions)) / n_sessions;	
		work_time = ptime * n_sessions;
	}

	for (i = 0; i < n_sessions; ++i) {
		if (i == 0) {
			mode = "first_pomodoro";
		} else {
			mode = "pomodoro";
		}

		notify(mode, n_sessions, ttime, work_time, i, ptime, sbktime, lbktime);
		countdown_timer(ptime * 60);
		log_pomodoro(ptime, category);

		if (i < n_sessions - 1) {
			if ((i + 1) % frequency == 0) {
				mode = "long_break";
				notify(mode, n_sessions, ttime, work_time, i, ptime, sbktime, lbktime);
				countdown_timer(lbktime * 60);
			} else {
				mode = "short_break";
				notify(mode, n_sessions, ttime, work_time, i, ptime, sbktime, lbktime);
				countdown_timer(sbktime * 60);
			}
		}
	}

	mode = "finished";
	notify(mode, 0, 0, 0, 0, 0, 0, 0);
}

void query_logs()
{
    char path[128];
    snprintf(path, sizeof(path), "%s%s%s", getenv("HOME"), CTIMER, "pomodoros.log");

    FILE *log = fopen(path, "r");
    if (!log) {
        fprintf(stderr, "Error opening log file\n");
        exit(EXIT_FAILURE);
    }

    // for weekly calculations
    time_t today = time(NULL);
    struct tm *timeinfo = localtime(&today);
    int current_day_of_week = timeinfo->tm_wday; 
    int days_since_monday = (current_day_of_week + 6) % 7; 
    time_t last_monday = today - days_since_monday * 86400; 
    time_t next_monday = last_monday + 7 * 86400;        

    // initialize vars
    char line[128];
    int log_count = 0;
    int buffer = 128;
    LogData *logs_full = malloc(buffer * sizeof(LogData));
    if (!logs_full) {
        perror("Failed to allocate memory to log struct");
        exit(EXIT_FAILURE);
    }
    int w_mins = 0;
    int w_cat_c = 0;
    int cat_buffer = 7;
    CatTimePair *w_cat_time = malloc(cat_buffer * sizeof(CatTimePair));
    if (!w_cat_time) {
        perror("Failed to allocate memory to category list");
        exit(EXIT_FAILURE);
    }
    int t_mins = 0;
    int t_cat_c = 0;
    CatTimePair *t_cat_time = malloc(cat_buffer * sizeof(CatTimePair));
    if (!t_cat_time) {
        perror("Failed to allocate memory to category list");
        exit(EXIT_FAILURE);
    }
    int minutes_per_day[7] = {0}; 

    // 1. fill up logs_full
    while (fgets(line, sizeof(line), log) != NULL) {
        int minutes;
        char category[64];
        long log_timestamp;
        if (sscanf(line, "%d|%63[^|]|%ld", &minutes, category, &log_timestamp) == 3) {
            if (log_count >= buffer) {
                buffer *= 2;
                logs_full = realloc(logs_full, buffer * sizeof(LogData));
                if (!logs_full) {
                    perror("Failed to reallocate memory to log struct");
                    exit(EXIT_FAILURE);
                }
            }

            logs_full[log_count].minutes = minutes;
            strncpy(logs_full[log_count].category, category, sizeof(logs_full[log_count].category) - 1);
            logs_full[log_count].category[sizeof(logs_full[log_count].category) - 1] = '\0';
            logs_full[log_count].timestamp = (time_t)log_timestamp;
            log_count++;
        }
    }

    // 2. process logs_full
    for (int i = 0; i < log_count; ++i) {
        t_mins += logs_full[i].minutes;

        bool match_found = false;
        for (int j = 0; j < t_cat_c; ++j) {
            if (strcmp(logs_full[i].category, t_cat_time[j].category) == 0) {
                match_found = true;
                t_cat_time[j].minutes += logs_full[i].minutes;
                break;
            }
        }
        if (!match_found) {
            if (t_cat_c >= cat_buffer) {
                cat_buffer *= 2;
                t_cat_time = realloc(t_cat_time, cat_buffer * sizeof(CatTimePair));
                if (!t_cat_time) {
                    perror("Failed to reallocate memory to category list");
                    exit(EXIT_FAILURE);
                }
            }
            t_cat_time[t_cat_c].category = strdup(logs_full[i].category);
            t_cat_time[t_cat_c].minutes = logs_full[i].minutes;
            t_cat_c++;
        }

        // 2.a. process current week's portion of logs_full
        if (logs_full[i].timestamp >= last_monday && logs_full[i].timestamp < next_monday) {
            w_mins += logs_full[i].minutes;

            match_found = false;
            for (int j = 0; j < w_cat_c; ++j) {
                if (strcmp(logs_full[i].category, w_cat_time[j].category) == 0) {
                    match_found = true;
                    w_cat_time[j].minutes += logs_full[i].minutes;
                    break;
                }
            }
            if (!match_found) {
                if (w_cat_c >= cat_buffer) {
                    cat_buffer *= 2;
                    w_cat_time = realloc(w_cat_time, cat_buffer * sizeof(CatTimePair));
                    if (!w_cat_time) {
                        perror("Failed to reallocate memory to category list");
                        exit(EXIT_FAILURE);
                    }
                }
                printf("New category [%s] found\n", logs_full[i].category);
                w_cat_time[w_cat_c].category = strdup(logs_full[i].category);
                w_cat_time[w_cat_c].minutes = logs_full[i].minutes;
                w_cat_c++;
            }

            struct tm *log_time = localtime(&logs_full[i].timestamp);
            int log_day_of_week = log_time->tm_wday; // 0 = Sunday, 6 = Saturday
            int index = (log_day_of_week + 6) % 7;   // 0 = Monday, 6 = Sunday
            minutes_per_day[index] += logs_full[i].minutes;
        }
    }

    // 3. print totals
    // 3.a. all-time
    printf("\nTOTALS: ALL-TIME\n");
    printf("----------------\n");
    printf("Total studied: %02dh:%02dm\n", t_mins / 60, t_mins % 60);
    int line_len = strlen("Total studied");
    for (int i = 0; i < t_cat_c; ++i) {
        int cat_len = strlen(t_cat_time[i].category);
        int total_pad = line_len - cat_len;
        if (total_pad > 0) {
            printf("%*s%s: %02dh:%02dm\n",
                   total_pad, "",
                   t_cat_time[i].category,
                   t_cat_time[i].minutes / 60,
                   t_cat_time[i].minutes % 60);
        } else {
            printf("\t%s: %02dh:%02dm\n",
                   t_cat_time[i].category,
                   t_cat_time[i].minutes / 60,
                   t_cat_time[i].minutes % 60);
        }
        free(t_cat_time[i].category);
    }
    free(t_cat_time);

    // 3.b. weekly
    printf("\nTOTALS: WEEKLY\n");
    printf("--------------\n");
    if (w_cat_c == 0) {
        printf("No logs found for the week\n");
    } else {
        printf("Total studied: %02dh:%02dm\n", w_mins / 60, w_mins % 60);
        for (int i = 0; i < w_cat_c; ++i) {
            printf("\t%s: %02dh:%02dm\n",
                   w_cat_time[i].category, w_cat_time[i].minutes / 60, w_cat_time[i].minutes % 60);
            free(w_cat_time[i].category);
        }
    }
    free(w_cat_time);

    // 3.c. weekly histogram
    printf("\nHISTOGRAM\n");
    printf("---------\n");
    const char *weekdays[] = {"Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday", "Sunday"};

    int max_len = strlen("Wednesday");

    int max_minutes = 0;
    for (int i = 0; i < 7; i++) {
        if (minutes_per_day[i] > max_minutes) {
            max_minutes = minutes_per_day[i];
        }
    }

    if (max_minutes > 0) {
        int total_days = days_since_monday + 1; 
        for (int i = 0; i < total_days; i++) {
            int padding = max_len - strlen(weekdays[i]);
            printf("%*s%s: ", padding, "", weekdays[i]);

            int num_hashes = (minutes_per_day[i] * 50) / max_minutes;
            for (int j = 0; j < num_hashes; j++) {
                printf("#");
            }
            if (minutes_per_day[i] > 0) {
                printf(" (%d minutes)\n", minutes_per_day[i]);
            } else {
                printf("\n");
            }
        }
    } else {
        printf("No logs found for the week\n");
    }

    free(logs_full);
    if (fclose(log) != 0) {
        perror("Failed to close the log file");
        exit(EXIT_FAILURE);
    }
}

void printhelp
(char *argv_0)
{

	fprintf(stderr,
			"Usage: %s\n"
			"   [-n number of pomodoros]\n"
			"   [-t pomodoro time]\n"
			"   [-T target session time]\n"
			"   [-s short break time]\n"
			"   [-l long break time]\n"
			"   [-f frequency (sessions before a long break)]\n"
			"   [-c category (for logging)]\n"
			"   [-h to print this message and exit]\n"
			"Use man ctimer for more detailed information\n"
			"Default: 5 pomodoros of 25 minutes with cycles of 4/1 of 5/15m breaks\n",
			argv_0);
	exit(EXIT_FAILURE);
}

int main
(int argc, char *argv[])
{
	int n_sessions = 5;
	int ptime = 25;
	int ttime = 0;
	int sbktime = 5;
	int lbktime = 15;
	int frequency = 4;
	char *category = NULL;

	int opt;
	while ((opt = getopt(argc, argv, "n:t:T:s:l:f:c:h")) != -1) {
		switch (opt) {
			case 'n':
				n_sessions = atoi(optarg);
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
				frequency = atoi(optarg);
				break;
			case 'c':
				category = optarg;
				break;
			case 'h':
				printhelp(argv[0]);
			default:
				printhelp(argv[0]);
		}
	}

	// the actual program
	pomodoro_timer(n_sessions, ptime, sbktime, lbktime, frequency, category, ttime);
	query_logs();

	exit(EXIT_SUCCESS);
}
