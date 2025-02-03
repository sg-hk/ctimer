# C-timer
#### a pomodoro timer in C

A pomodoro timer that takes on options through flags. 

Uses [herbe](https://github.com/dudik/herbe/) for notifications, and printfs the countdown to stdout. Creates daily log as well. Sound files and log in ```~/.local/share/ctimer ```.

```-h``` or ```man ctimer``` for more information.

To install:
```make && make install```


You could do the following to get the countdown in your status bar:
```sh
mkfifo /tmp/ctimer
ctimer > /tmp/ctimer &
while read line; do
    xsetroot -name "$line"
done < /tmp/ctimer
```
