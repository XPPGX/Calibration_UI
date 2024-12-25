#include <stdio.h>
#include <wiringPi.h>
#include <unistd.h>
#include <pthread.h>

#define GPIO_PIN 0

int led_status = 0;
int my_func(int a, double b){
    return a + (int)b;
}

void init_gpio(){
    if(wiringPiSetup() == -1){
        printf("Failed to initialize WiringPi\n");
        return;
    }
    pinMode(GPIO_PIN, OUTPUT);
}

void set_gpio(int value){
    digitalWrite(GPIO_PIN, value);
}

void blink_led(){
    if(led_status == 0){
        led_status = 1;
        set_gpio(1);
    }
    else{
        led_status = 0;
        set_gpio(0);
    }
}

void* thread_task(void *arg){
    init_gpio();
    while(1){
        sleep(1);
        blink_led();
    }
}

int get_led_status(){
    return led_status;
}

void entry(){
    pthread_t t1;
    pthread_create(&t1, NULL, thread_task, NULL);
}
