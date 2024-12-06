# gcc test.c -o a -lX11 -I/usr/include/freetype2 -lXft -lXrender -lfontconfig -lfreetype
gcc UI_layout_main.c -o a $(pkg-config --cflags --libs gtk+-3.0) -lwiringPi -pthread
#gcc Cali.c -o b $(pkg-config --cflags --libs gtk+-3.0) -lwiringPi -pthread
