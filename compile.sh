gcc UI.c Cali.c -o main -pthread -lwiringPi $(pkg-config --cflags --libs gtk+-3.0)
