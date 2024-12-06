#ifndef Cali
#define Cali
#include "Cali.h"
#endif

#include "UI_layout.c"


GMutex mutex_interface;
GtkWidget *label;
int space_pressed = 0;

gboolean update_label(gpointer data){
    const char *message = (const char*)data;
    gtk_label_set_text(GTK_LABEL(label), message);
    return FALSE;
}

int main(int argc, char* argv[]){
    
    GtkApplication *app;
    int status;
    app = gtk_application_new("Meanwell.com", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return 0;
}
