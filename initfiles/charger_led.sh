#!/system/bin/sh

sleep 3

chmod 0777 /sys/class/leds/red/brightness
chmod 0777 /sys/class/leds/green/brightness
chmod 0777 /sys/class/leds/blue/brightness

echo "none" > /sys/class/leds/red/trigger
echo "none" > /sys/class/leds/green/trigger
echo "none" > /sys/class/leds/blue/trigger

echo 255 > /sys/class/leds/red/brightness
echo 0 > /sys/class/leds/green/brightness
echo 0 > /sys/class/leds/blue/brightness

while true; do

    STATUS=$(cat /sys/class/power_supply/battery/status 2>/dev/null || cat /sys/class/power_supply/main-battery/status 2>/dev/null || echo 'Unknown')

    CAP=$(cat /sys/class/power_supply/battery/capacity 2>/dev/null || cat /sys/class/power_supply/main-battery/capacity 2>/dev/null || echo 0)
    
    if [ "$STATUS" = 'Charging' ] || [ "$STATUS" = 'Full' ]; then
        if [ $CAP -ge 90 ]; then
            echo 0 > /sys/class/leds/red/brightness
            echo 255 > /sys/class/leds/green/brightness
        else
            echo 255 > /sys/class/leds/red/brightness
            echo 0 > /sys/class/leds/green/brightness
        fi
    fi

    sleep 5
done