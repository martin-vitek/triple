sudo update-rc.d -f run_tripled remove
sudo rm -f /usr/sbin/tripled_64
sudo rm -f /etc/init.d/run_tripled
sudo rm -f /etc/init.d/usb2cansocketcan.ko
#rm /lib/modules/$(uname -r)/kernel/drivers/net/can/usb2cansocketcan.ko
