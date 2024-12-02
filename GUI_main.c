#ifndef GTK_LIB
#define GTK_LIB
#include <gtk/gtk.h>
#endif

#ifndef _SHARE_
#define _SHARE_
#include "share_variables.h"
#endif

#include "EventCallback.c"

void init_share_var(void){
    adjustment_status = 0;
}

//UI layout and event signal connecting
static void on_activate(GtkApplication *app, gpointer user_data) {
    //Initial shared variables
    init_share_var();

    // Create Main window
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "校正治具");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 500);

    // Create Labels
    GtkWidget *label1 = gtk_label_new("測試機型");
    GtkWidget *label2 = gtk_label_new("通訊界面");
    GtkWidget *label3 = gtk_label_new("校正種類");

    GtkWidget *frame_adjust_mode = gtk_frame_new(NULL);
    GtkWidget *label_adjust_mode = gtk_label_new("粗調");
    gtk_widget_set_size_request(label_adjust_mode, 30, 10);
    gtk_container_add(GTK_CONTAINER(frame_adjust_mode), label_adjust_mode);
    gtk_widget_set_size_request(frame_adjust_mode, 30, 10);

    // Create Buttons
    GtkWidget *button_setting = gtk_button_new_with_label("開始設定");
    gtk_widget_set_can_focus(button_setting, FALSE); //
    gtk_widget_set_size_request(button_setting, 30, 45);
    
    // Create boxes
    GtkWidget *vbox     = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10); // 10是控件之间的间距
    GtkWidget *vbox1    = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); 
    GtkWidget *hbox[5];
    for(int i = 0 ; i < 5 ; i ++){
        hbox[i] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_size_request(hbox[i], 200, 60);
    }

    // MT : Machine Type
    GtkWidget *frame_MT = gtk_frame_new(NULL);
    GtkWidget *label_MT = gtk_label_new("-");
    gtk_container_add(GTK_CONTAINER(frame_MT), label_MT);
    gtk_widget_set_size_request(frame_MT, 100, 60);
    // CI : Communication Interface
    GtkWidget *frame_CI = gtk_frame_new(NULL);
    GtkWidget *label_CI = gtk_label_new("-");
    gtk_container_add(GTK_CONTAINER(frame_CI), label_CI);
    gtk_widget_set_size_request(frame_CI, 100, 60);
    // CK : Callibration Kind
    GtkWidget *frame_CK = gtk_frame_new(NULL);
    GtkWidget *label_CK = gtk_label_new("-");
    gtk_container_add(GTK_CONTAINER(frame_CK), label_CK);
    gtk_widget_set_size_request(frame_CK, 100, 60);
    // SS : Setting step
    GtkWidget *frame_SS = gtk_frame_new("顯示設定步驟");
    GtkWidget *label_SS = gtk_label_new("-");
    gtk_container_add(GTK_CONTAINER(frame_SS), label_SS);
    gtk_widget_set_size_request(frame_SS, 115, 70);
    // R : Result
    GtkWidget *frame_R = gtk_frame_new("顯示結果");
    GtkWidget *label_R = gtk_label_new("-");
    gtk_container_add(GTK_CONTAINER(frame_R), label_R);
    gtk_widget_set_size_request(frame_R, 222, 100);

    // Set font type
    PangoFontDescription *font_desc = pango_font_description_from_string("Noto Sans CJK SC 10");
    gtk_widget_override_font(label1, font_desc);
    gtk_widget_override_font(label2, font_desc);
    gtk_widget_override_font(label3, font_desc);
    gtk_widget_override_font(label_MT, font_desc);
    gtk_widget_override_font(label_CI, font_desc);
    gtk_widget_override_font(label_CK, font_desc);
    gtk_widget_override_font(frame_SS, font_desc);
    gtk_widget_override_font(label_SS, font_desc);
    gtk_widget_override_font(frame_R, font_desc);
    gtk_widget_override_font(label_R, font_desc);
    gtk_widget_override_font(button_setting, font_desc);
    gtk_widget_override_font(label_adjust_mode, font_desc);
    pango_font_description_free(font_desc);

    
    gtk_box_pack_start(GTK_BOX(vbox), hbox[0], FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox[1], FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox[2], FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox[3], FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox[4], FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(hbox[0]), label1, FALSE, TRUE, 20);
    gtk_box_pack_start(GTK_BOX(hbox[0]), frame_MT, FALSE, TRUE, 20);
    gtk_box_pack_start(GTK_BOX(hbox[1]), label2, FALSE, TRUE, 20);
    gtk_box_pack_start(GTK_BOX(hbox[1]), frame_CI, FALSE, TRUE, 20);
    gtk_box_pack_start(GTK_BOX(hbox[2]), label3, FALSE, TRUE, 20);
    gtk_box_pack_start(GTK_BOX(hbox[2]), frame_CK, FALSE, TRUE, 20);
    gtk_box_pack_start(GTK_BOX(vbox1), frame_adjust_mode, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), button_setting, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox[3]), GTK_BOX(vbox1), FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(hbox[3]), frame_SS, FALSE, FALSE, 5);
    gtk_box_pack_start(GTK_BOX(hbox[4]), frame_R, FALSE, TRUE, 5);

    //Signal
    g_signal_connect(button_setting, "clicked", G_CALLBACK(CB_button_setting_click), label_SS);
    g_signal_connect(window, "key-press-event", G_CALLBACK(CB_key_space), label_adjust_mode);
    

    gtk_container_add(GTK_CONTAINER(window), vbox); // put the box on the screen
    gtk_widget_show_all(window); //show the window on the screen
}

int main(int argc, char *argv[]) {
    // 创建应用程序
    GtkApplication *app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);

    // 运行应用程序
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
