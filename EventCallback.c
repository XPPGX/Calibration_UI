#ifndef GTK_LIB
#define GTK_LIB
#include <gtk/gtk.h>
#endif

#ifndef _SHARE_
#define _SHARE_
#include "share_variables.h"
#endif

extern void CB_button_setting_click(GtkButton *button, gpointer user_data) {
    GtkLabel *label = GTK_LABEL(user_data); // 获取传入的标签
    gtk_label_set_text(label, "O口O"); // 更新标签内容
}

extern gboolean CB_key_space(GtkWidget *widget, GdkEventKey *event, gpointer user_data){
    
    if(event->keyval == GDK_KEY_space){
        // g_print("SPACE key pressed\n");
        GtkLabel *label = GTK_LABEL(user_data);
        if(adjustment_status == 0){
            adjustment_status = 1;
            gtk_label_set_text(label, "細調");
        }
        else{
            adjustment_status = 0;
            gtk_label_set_text(label, "粗調");
        }
    }
    return FALSE;
}
