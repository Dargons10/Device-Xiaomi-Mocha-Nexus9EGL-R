#!/system/bin/sh
# Storage FUSE watchdog: restarts vold if emulated storage becomes inaccessible
CHECK_INTERVAL=30
MAX_RETRIES=3

while true; do
    sleep $CHECK_INTERVAL

    # Check if emulated storage is accessible
    if ! ls /storage/emulated/0/ > /dev/null 2>&1; then
        RETRIES=0
        while [ $RETRIES -lt $MAX_RETRIES ]; do
            RETRIES=$((RETRIES + 1))
            # Kill vold - init will restart it automatically
            VOLD_PID=$(pidof vold 2>/dev/null)
            if [ -n "$VOLD_PID" ]; then
                kill -9 $VOLD_PID 2>/dev/null
            fi
            sleep 5
            if ls /storage/emulated/0/ > /dev/null 2>&1; then
                break
            fi
        done
    fi
done
