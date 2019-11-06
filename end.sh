sudo pkill -2 tripled_64
sleep 0.2
sudo rmmod usb2cansocketcan
sleep 0.5
rm /lib/modules/$(uname -r)/kernel/drivers/net/can/usb2cansocketcan.ko
