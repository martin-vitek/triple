sudo rm -f /usr/sbin/tripled_64
sudo rm -f /etc/init.d/run_tripled
sudo rm -f /etc/init.d/usb2cansocketcan.ko
sudo cp ../tripled_64 /usr/sbin
sudo cp ./run_tripled /etc/init.d
sudo cp ../usb2cansocketcan.ko /etc/init.d
#sudo cp ../usb2cansocketcan.ko /lib/modules/$(uname -r)/kernel/drivers/net/can
#sudo depmod -a
sudo chmod +x /etc/init.d/run_tripled
sudo update-rc.d run_tripled start 1 2 3 4 5 . stop 0 6 .
