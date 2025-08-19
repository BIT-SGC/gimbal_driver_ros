#!/bin/bash
# ~/.local/bin/switch-stream.sh

PIPE="/tmp/stream-control-pipe"
PID_FILE="/tmp/gst-switch.pid"
UDP_HOST="192.168.43.233"
UDP_PORT="5600"

CURRENT_SOURCE=""

# 清理已有进程
cleanup() {
    if [[ -f "$PID_FILE" ]]; then
        kill "$(cat "$PID_FILE")" 2>/dev/null || true
        rm -f "$PID_FILE"
    fi
    pkill -f "gst-launch-1.0.*rtspsrc" 2>/dev/null || true
}

# 启动指定源
start_source() {
    local src=$1
    local uri
    case "$src" in
        "1")
            uri="rtsp://192.168.144.108:554/stream=1"
            ;;
        "2")
            uri="rtsp://192.168.144.108:555/stream=2"
            ;;
        *)
            echo "Invalid source: $src"
            return 1
            ;;
    esac

    cleanup

    echo "Starting source $src: $uri"
    gst-launch-1.0 \
        rtspsrc location="$uri" latency=0 \
        ! application/x-rtp,media=video,encoding-name=H265 \
        ! rtph265depay \
        ! rtph265pay config-interval=1 \
        ! udpsink host="$UDP_HOST" port="$UDP_PORT" sync=false async=false &

    echo $! > "$PID_FILE"
    CURRENT_SOURCE="$src"
}

# 初始化 FIFO
setup_pipe() {
    if [[ ! -p "$PIPE" ]]; then
        rm -f "$PIPE"
        mkfifo "$PIPE"
    fi
}

# 主循环
main() {
    setup_pipe
    start_source "1"  # 默认启动源1

    echo "Listening on $PIPE for commands: 'switch 1', 'switch 2', 'quit'"

    while true; do
        if read cmd < "$PIPE"; then
            case "$cmd" in
                "switch 1"|"switch 2")
                    src="${cmd##* }"
                    start_source "$src"
                    ;;
                "quit")
                    cleanup
                    rm -f "$PIPE"
                    exit 0
                    ;;
                *)
                    echo "Unknown command: $cmd"
                    ;;
            esac
        fi
    done
}

# 启动主逻辑
main "$@"