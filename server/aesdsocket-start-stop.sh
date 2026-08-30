#!/bin/sh

case "$1" in
    start)
        echo "Starting aesdsocket"
        # Start the aesdsocket daemon in the background
        start-stop-daemon -S -b -n aesdsocket aesdsocket -- -d
        ;;
    stop)
        echo "Stopping aesdsocket"
        start-stop-daemon -K -n aesdsocket
        ;;
    *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac