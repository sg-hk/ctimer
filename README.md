# ctimer
#### portable and configurable CLI pomodoro timer

Defaults to 5 pomodoros of 25 minutes, with 5-minute short breaks 
and a 15-minute long break every 4 sessions.

Uses [mpv](https://mpv.io/) for sound and [herbe](https://github.com/dudik/herbe/) 
or system defaults (Unix, Windows) for notifications. 
Prints the countdown to stdout or writes it to a named pipe if `-F` is set. 

Stores sound files in `~/.local/share/ctimer`.

### Usage
See `-h` or `man ctimer`.

### Installation
```sh
make && sudo make install
```

Or for herbe:
```sh
make ENABLE_HERBE=1 && sudo make install
```

To uninstall
```sh
make clean && sudo make uninstall
```
