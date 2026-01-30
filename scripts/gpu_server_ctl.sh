#!/bin/bash
# GPU Server Management Script

RPC_DIR="/data/IRP-first/IRP-first/rpc"
PORT=50051

status() {
    echo "=== Port Listening Status ==="
    netstat -tlnp 2>/dev/null | grep $PORT || echo "Port $PORT is not being listened"
    echo ""

    echo "=== Process List ==="
    ps aux | grep "python3.*server.py" | grep -v grep || echo "No running GPU server process"
    echo ""

    # GPU usage
    if command -v nvidia-smi &> /dev/null; then
        echo "=== GPU Usage ==="
        nvidia-smi --query-gpu=index,name,memory.used,memory.total,utilization.gpu --format=csv
    fi
}

start() {
    # Check if already running
    if netstat -tlnp 2>/dev/null | grep -q $PORT; then
        echo "[WARNING] GPU server is already running"
        status
        return 1
    fi

    echo "[INFO] Starting GPU server..."
    cd "$RPC_DIR"
    nohup python3 server.py > server.log 2>&1 &
    local pid=$!
    echo "[OK] GPU server started (PID: $pid)"
    sleep 2
    status
}

stop() {
    echo "[INFO] Stopping GPU server..."
    local pids=$(ps aux | grep "python3.*server.py" | grep -v grep | awk '{print $2}')

    if [ -z "$pids" ]; then
        echo "[INFO] No running GPU server process"
        return 0
    fi

    for pid in $pids; do
        echo "   Stopping process $pid"
        kill -15 $pid 2>/dev/null
    done

    sleep 2

    # Check for remaining processes
    pids=$(ps aux | grep "python3.*server.py" | grep -v grep | awk '{print $2}')
    if [ -n "$pids" ]; then
        echo "[WARNING] Some processes still running, force killing..."
        for pid in $pids; do
            kill -9 $pid 2>/dev/null
        done
    fi

    echo "[OK] GPU server stopped"
}

restart() {
    stop
    sleep 2
    start
}

case "$1" in
    status|st)
        status
        ;;
    start)
        start
        ;;
    stop)
        stop
        ;;
    restart|rs)
        restart
        ;;
    *)
        echo "Usage: $0 {status|start|stop|restart}"
        echo ""
        echo "Commands:"
        echo "  status   (st)  - View GPU server status"
        echo "  start          - Start GPU server"
        echo "  stop           - Stop GPU server"
        echo "  restart  (rs)  - Restart GPU server"
        exit 1
        ;;
esac
