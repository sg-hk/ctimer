# C-timer
#### a pomodoro timer in C

A pomodoro timer that takes on options through flags. Uses [herbe](https://github.com/dudik/herbe/) for notifications, and printfs the countdown to stdout. Creates daily log as well.

```-h``` or ```man ctimer``` for more information.

Sound files made by me using online synth and Audacity. They are found in ~/.local/share/ctimer. Feel free to change them as you like.

Logs are in the same directory.

To install:
```make && make install```


You could do the following to get the countdown in your status bar:
```
mkfifo /tmp/ctimer
ctimer > /tmp/ctimer &
while read line; do
    xsetroot -name "$line"
done < /tmp/ctimer
```
