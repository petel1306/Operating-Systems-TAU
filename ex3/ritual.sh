clear
cd ~/os/
sudo chmod -R 777 ./ex3 
cd ./ex3/code
sudo rmmod message_slot
make clean
bear bash -c ./make.sh
sudo insmod message_slot.ko
sudo rm /dev/msgslot1
sudo mknod /dev/msgslot1 c 240 1
sudo chmod o+rw /dev/msgslot1
# ./message_sender /dev/msgslot1 4 "Hello this is channel 4"
# ./message_reader /dev/msgslot1 4
cd ..