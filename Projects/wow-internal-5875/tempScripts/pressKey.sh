#!/bin/bash

echo "Letar automatiskt efter WoW."
echo "Skickar W var 10:e minut."
echo "Avsluta med Ctrl+C."

while true; do
    WINDOW_HEX=$(wmctrl -lx | awk '$3 == "wow.exe.wow.exe" {print $1; exit}')

    if [ -z "$WINDOW_HEX" ]; then
        echo "$(date '+%H:%M:%S') - WoW hittades inte, försöker igen om 5 sekunder..."
        sleep 5
        continue
    fi

    WINDOW=$((WINDOW_HEX))

    echo "$(date '+%H:%M:%S') - WoW hittad: $WINDOW_HEX - skickar W"
    xdotool key --window "$WINDOW" w

    sleep 600
done
