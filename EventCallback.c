#ifndef Cali
#define Cali
#include "Cali.h"
#endif

pthread_mutex_t button_lock;
pthread_cond_t button_cond;

extern void CB_button_setting_click(GtkButton *button, gpointer user_data) {
    GtkLabel *label = GTK_LABEL(user_data);
    pthread_cond_signal(&button_cond);
    gtk_label_set_text(label, "O口O");
}

extern void CB_timer(GtkWidget *label){
    if(space_pressed == 0){
        gtk_label_set_text(GTK_LABEL(label), "微調");
    }
    else{
        gtk_label_set_text(GTK_LABEL(label), "粗調");
    }
}

extern void read_keyboard_input_test(){

    pthread_mutex_lock(&button_lock);
    printf("[keyboard_thread]Waiting for button pressing...\n");
    pthread_cond_wait(&button_cond, &button_lock);
    pthread_mutex_unlock(&button_lock);
    printf("[keyboard_thread]Start\n");

    int SVR_value = 0;
    int quit_flag = 0; //1 for leaving the while block, 0 otherwise
    const char *device = "/dev/input/event0";
    int fd = open(device, O_RDONLY);
    space_pressed = 0; // 記錄空白鍵的狀態
    
    if (fd == -1) {
        perror("無法開啟輸入設備");
        return EXIT_FAILURE;
    }

    // 設置終端屬性來禁用輸入回顯
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);           // 獲取當前終端設置
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);         // 禁用行緩衝和回顯
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);  // 設置新的終端設置

    struct input_event ev;
    printf("開始監控鍵盤輸入（使用上下左右鍵進行控制，按 Ctrl+C 結束）\n");

    while (1) {
        // Read events
        read(fd, &ev, sizeof(struct input_event));

         // Only handle key press events
        if (ev.type == EV_KEY) {
            switch (ev.code) {
                case KEY_SPACE:
                    // update adjustment(1:Coarse 0:Fine)
                    if (ev.value == 1) {
                        space_pressed = !space_pressed; //1:Coarse 0:Fine
                        printf("Space");
                    }
                    break;
                    
                case KEY_RIGHT:
                    printf("Right");
                    if (ev.value == 1) { // press right
                        if (space_pressed == 1) {
                            // Coarse
                            SVR_value += 50;
                        } else {
                            // Fine
                            SVR_value += 1;
                        }
                    }
                    break;

                case KEY_LEFT:
                    printf("Left");
                    if (ev.value == 1) { // press left
                        if (space_pressed == 1) {
                            // Coarse
                            SVR_value -= 50;
                        } else {
                            // Fine
                            SVR_value -= 1;
                        }
                    }
                    break;
                
                case KEY_ENTER:
                    quit_flag = 1;
                    printf("enter");
                    break;
            }
            if(SVR_value < 0)
            {
                SVR_value = 0;
            }
            else if(SVR_value > 4095)
            {
                SVR_value = 4095;
            }
            printf("%d\n", space_pressed);
            printf("%d\n", SVR_value);
        }

        if(quit_flag == 1){
            break;
        }
    }

    // Restore terminal settings
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    close(fd);
}

extern gboolean CB_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data){
    switch(event->keyval){
        case GDK_KEY_space:
            g_print("SPACE\n");
            GtkLabel *label = GTK_LABEL(user_data);
            if(adjustment_status == 0){
                adjustment_status = 1;
                gtk_label_set_text(label, "細調");
            }
            else{
                adjustment_status = 0;
                gtk_label_set_text(label, "粗調");
            }
            break;
            
        case GDK_KEY_Left:
            g_printf("LEFT\n");
            break;
            
        case GDK_KEY_Right:
            g_printf("Right\n");
            break;
            
        case GDK_KEY_Return:
            g_printf("Enter\n");
            break;
    }
    
    return FALSE;
}

extern void on_destroy(GtkWidget *widget, gpointer data){
    
    gtk_main_quit();
}