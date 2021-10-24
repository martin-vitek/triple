# USB2CAN

Driver was tested on ubuntu 18.04

SLCAN like driver for PIC based USB to CAN adapter USB2CAN Triple from Canlab s.r.o.
For more info https://github.com/canlab-cz/triple/wiki/Manual

You can find HW here : www.canlab.cz

Easy start :
1. `git clone https://github.com/canlab-cz/triple`
2. `cd triple`
3. `make`
4. `sh ./start.sh`

To kill and unload all `sh ./end.sh`

## TODO
1) Test on newer distributions...
2) Use docker or some virtual env for providing tools for easier testing
3) ..
