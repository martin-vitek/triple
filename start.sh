sudo pkill -2 tripled_64
sleep 0.2
sudo rmmod usb2cansocketcan
sudo insmod usb2cansocketcan.ko
sudo cp usb2cansocketcan.ko /lib/modules/$(uname -r)/kernel/drivers/net/can
sudo depmod -a
sudo modprobe usb2cansocketcan
sudo ./tripled_64 -s250 ttyACM0 can0
sudo sudo ip link set can0 up qlen 1000
