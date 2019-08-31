#! /bin/bash
# Heather Lomond 02/June/2019

PATHBIN="/home/pi/rpidatv/bin/"

sudo killall -9 hello_video.bin
sudo killall ts2es
sudo killall fake_read
sudo killall longmynd
sudo killall nc

mkfifo fifo.264
mkfifo longmynd_main_ts
mkfifo longmynd_main_status

/home/pi/longmynd/fake_read &	

# Version to display via UDP on a remote machine
#sudo /home/pi/longmynd/longmynd -i 192.168.1.9 1234 436868 250 &

# Version to display on local display
sudo /home/pi/longmynd/longmynd 437168 250 &
$PATHBIN"ts2es" -video longmynd_main_ts fifo.264 &
$PATHBIN"hello_video.bin" fifo.264 &
