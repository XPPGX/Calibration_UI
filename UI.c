
#include "Cali.h"
#include "UI.h"


GtkWidget* label_MT;
GtkWidget* label_CI;
GtkWidget* label_CT;
GtkWidget *label_SS;
GtkWidget* label_R;
GtkWidget *label_adjust_mode;

const char* Rcv_MachineType;
char Rcv_CommInterface = 0;
char Rcv_CaliType = 0;
int UI_Init_Flag = 0;
int CaliTypeFlag = 0;
int timer_counter = 0;
volatile uint8_t Rcv_CaliStatus = 0;
int Rcv_CaliTypePointOld = 0;
int Rcv_CaliTypePointNew = 0;
char Rcv_CaliTypePointStr[20] = "";
int UI_ButtonPressed_Flag = 0;



void on_activate(GtkApplication *app, gpointer user_data);
void CB_button_setting_click(GtkButton *button, gpointer user_data);
gboolean CB_timer();
gboolean CB_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data);
void UI_StartCali();


int main(int argc, char *argv[]){
	GtkApplication *app;
    int status;
    app = gtk_application_new("Meanwell.com", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
}


void on_activate(GtkApplication *app, gpointer user_data) {
    
    // Create Main window
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "校正治具");
    gtk_window_set_default_size(GTK_WINDOW(window), WINDOW_WIDTH, WINDOW_HEIGHT);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER_ALWAYS);
    
    // Create Labels
    GtkWidget *label1 = gtk_label_new("測試機型");
    GtkWidget *label2 = gtk_label_new("通訊界面");
    GtkWidget *label3 = gtk_label_new("校正種類");

    GtkWidget *frame_adjust_mode = gtk_frame_new(NULL);
    label_adjust_mode = gtk_label_new("粗調");
    gtk_widget_set_size_request(label_adjust_mode, 120, 40);
    gtk_container_add(GTK_CONTAINER(frame_adjust_mode), label_adjust_mode);
    gtk_widget_set_size_request(frame_adjust_mode, 120, 40);

    // Create Buttons
    GtkWidget *button_setting = gtk_button_new_with_label("開始設定");
    gtk_widget_set_can_focus(button_setting, FALSE);
    gtk_widget_set_size_request(button_setting, 100, 45);
    
    // Create boxes
    GtkWidget *hbox_Whole = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 50);
    GtkWidget *vbox_left  = gtk_box_new(GTK_ORIENTATION_VERTICAL, 60); //[ADD To]60 is the distance between each widgets in the vbox_left
    GtkWidget *vbox_right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 60);
    GtkWidget *vbox1    = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0); 
    gtk_widget_set_size_request(vbox1, 30, 60);
    GtkWidget *hbox[5];
    for(int i = 0 ; i < 5 ; i ++){
        hbox[i] = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
        gtk_widget_set_size_request(hbox[i], 250, 60);
    }

    // MT : Machine Type
    GtkWidget *frame_MT = gtk_frame_new(NULL);
    label_MT = gtk_label_new("-");
    gtk_container_add(GTK_CONTAINER(frame_MT), label_MT);
    gtk_widget_set_size_request(frame_MT, 320, 60);
    // CI : Communication Interface
    GtkWidget *frame_CI = gtk_frame_new(NULL);
    label_CI = gtk_label_new("-");
    gtk_container_add(GTK_CONTAINER(frame_CI), label_CI);
    gtk_widget_set_size_request(frame_CI, 320, 60);
    // CT : Callibration Type
    GtkWidget *frame_CT = gtk_frame_new(NULL);
    label_CT = gtk_label_new("-");
    gtk_container_add(GTK_CONTAINER(frame_CT), label_CT);
    gtk_widget_set_size_request(frame_CT, 320, 60);
    // SS : Setting step
    GtkWidget *frame_SS = gtk_frame_new("校正點");
    label_SS = gtk_label_new("-");
    gtk_container_add(GTK_CONTAINER(frame_SS), label_SS);
    gtk_widget_set_size_request(frame_SS, 320, 70);
    // R : Result
    GtkWidget *frame_R = gtk_frame_new("顯示結果");
    label_R = gtk_label_new("-");
    gtk_container_add(GTK_CONTAINER(frame_R), label_R);
    gtk_widget_set_size_request(frame_R, 510, 150);

    // Set font type
    PangoFontDescription *font_desc = pango_font_description_from_string("Noto Sans CJK SC 24");
    gtk_widget_override_font(label1, font_desc);
    gtk_widget_override_font(label2, font_desc);
    gtk_widget_override_font(label3, font_desc);
    gtk_widget_override_font(label_MT, font_desc);
    gtk_widget_override_font(label_CI, font_desc);
    gtk_widget_override_font(label_CT, font_desc);
    gtk_widget_override_font(frame_SS, font_desc);
    gtk_widget_override_font(label_SS, font_desc);
    gtk_widget_override_font(frame_R, font_desc);
    gtk_widget_override_font(label_R, font_desc);
    gtk_widget_override_font(button_setting, font_desc);
    gtk_widget_override_font(label_adjust_mode, font_desc);
    pango_font_description_free(font_desc);

    //Put each widget into box
    gtk_box_pack_start(GTK_BOX(hbox_Whole), vbox_left, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(hbox_Whole), vbox_right, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(vbox_left), hbox[0], FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vbox_left), hbox[1], FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vbox_left), hbox[2], FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vbox_left), hbox[3], FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(vbox_left), hbox[4], FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(hbox[0]), label1, FALSE, TRUE, 20);
    gtk_box_pack_start(GTK_BOX(hbox[0]), frame_MT, FALSE, TRUE, 20);
    gtk_box_pack_start(GTK_BOX(hbox[1]), label2, FALSE, TRUE, 20);
    gtk_box_pack_start(GTK_BOX(hbox[1]), frame_CI, FALSE, TRUE, 20);
    gtk_box_pack_start(GTK_BOX(hbox[2]), label3, FALSE, TRUE, 20);
    gtk_box_pack_start(GTK_BOX(hbox[2]), frame_CT, FALSE, TRUE, 20);
    gtk_box_pack_start(GTK_BOX(vbox1), frame_adjust_mode, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox1), button_setting, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(hbox[3]), GTK_BOX(vbox1), FALSE, FALSE, 6);
    gtk_box_pack_start(GTK_BOX(hbox[3]), frame_SS, FALSE, FALSE, 10);
    gtk_box_pack_start(GTK_BOX(hbox[4]), frame_R, FALSE, TRUE, 5);

	Rcv_MachineType = Get_Machine_Name();
	
    //Signal
    g_signal_connect(button_setting, "clicked", G_CALLBACK(CB_button_setting_click), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(CB_key_press), NULL);
    
    //~ gtk_widget_set_events(window, GDK_ALL_EVENTS_MASK & ~GDK_KEY_PRESS_MASK);
    // g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), NULL);
    
    
    
    //Use timer to check the global variable to update UI
    g_timeout_add(100, (GSourceFunc)CB_timer, label_adjust_mode);

    gtk_container_add(GTK_CONTAINER(window), hbox_Whole); // put the box on the screen
    gtk_widget_show_all(window); //show the window on the screen
}

void CB_button_setting_click(GtkButton *button, gpointer user_data) {
	if(UI_ButtonPressed_Flag == 0){
		UI_ButtonPressed_Flag = 1;
		UI_StartCali();
	}
}

gboolean CB_timer(){

	//update Machine Type Label
	gtk_label_set_text(label_MT, Rcv_MachineType);
	if(UI_Init_Flag == 0){
		
		//update Communication Type Label
		Rcv_CommInterface = Get_Communication_Type();
		switch(Rcv_CommInterface){
			case CANBUS:
				gtk_label_set_text(label_CI, "CANBUS");
				UI_Init_Flag = 1;
				break;
			case MODBUS:
				gtk_label_set_text(label_CI, "MODBUS");
				UI_Init_Flag = 1;
				break;
			default:
				break;
		}
	}
	
	if(UI_Init_Flag == 1){
		Rcv_CaliType = Get_PSU_Calibration_Type();
		switch(Rcv_CaliType){
			case ACV_DC_OFFSET_3phi:
				gtk_label_set_text(GTK_LABEL(label_CT), "ACV_DC_OFFSET_3\u03A6");
				break;
			case ACV_DC_OFFSET_1phi:
				gtk_label_set_text(GTK_LABEL(label_CT), "ACV_DC_OFFSET_1\u03A6");
				break;
			case ACI_DC_OFFSET_3phi:
				gtk_label_set_text(GTK_LABEL(label_CT), "ACI_DC_OFFSET_3\u03A6");
				break;
			case ACI_DC_OFFSET_1phi:
				gtk_label_set_text(GTK_LABEL(label_CT), "ACI_DC_OFFSET_1\u03A6");
				break;
			case ACI_3phi:
				gtk_label_set_text(GTK_LABEL(label_CT), "ACI_3\u03A6");
				break;
			case ACI_1phi:
				gtk_label_set_text(GTK_LABEL(label_CT), "ACI_1\u03A6");
				break;
			case DC:
				gtk_label_set_text(GTK_LABEL(label_CT), "DC");
				break;
			default:
				break;
		}
		
		
		Rcv_CaliTypePointNew = Get_Calibration_Type_Point();
		printf("Rcv_CaliTypePointNew = %x\n", Rcv_CaliTypePointOld);
		if(Rcv_CaliTypePointNew != Rcv_CaliTypePointOld){
			Rcv_CaliTypePointOld = Rcv_CaliTypePointNew;
			memcpy(Rcv_CaliTypePointStr, "", sizeof(char) * 20);
			sprintf(Rcv_CaliTypePointStr, "%d", Rcv_CaliTypePointOld);
			gtk_label_set_text(GTK_LABEL(label_SS), Rcv_CaliTypePointStr);
		}
	}
	
	
	uint8_t Rcv_SpaceStatus = Get_Keyboard_Adjustment();
    if(Rcv_SpaceStatus == 0){
        gtk_label_set_markup(label_adjust_mode, "<span font='18' foreground='#000000'><b>微調</b></span>");
    }
    else{
        gtk_label_set_markup(label_adjust_mode, "<span font='18' foreground='#D12E2E'><b>粗調</b></span>"); //just set text with some style
    }
    
    // timer_counter ++;
    Rcv_CaliStatus = Get_Calibration_Status();
    switch(Rcv_CaliStatus){
		case FINISH:
			gtk_label_set_markup(label_R, "<span font='24' foreground='#00B9FF'><b>FINISH</b></span>");
			Stop_Cali_thread();
			UI_ButtonPressed_Flag = 0;
			break;
		case FAIL:
			gtk_label_set_markup(label_R, "<span font='24' foreground='#FF3F00'><b>FAIL</b></span>");
			Stop_Cali_thread();
			UI_ButtonPressed_Flag = 0;
			break;
	}
    
    // if(timer_counter >= 10){
	// 	timer_counter = 0;
	// 	//~ printf("[Timer]Cali_Status = %d\n", Rcv_CaliStatus);
	// }
	
	return TRUE;
}

gboolean CB_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data){
    if(UI_ButtonPressed_Flag == 0){
		if(event->keyval == GDK_KEY_space){
			UI_ButtonPressed_Flag = 1;
			UI_StartCali();  
		}
	}
    return FALSE;
}

void UI_StartCali(){
	gtk_label_set_text(label_MT, "-");
    gtk_label_set_text(label_CI, "-");
    gtk_label_set_text(label_CT, "-");
    gtk_label_set_text(label_SS, "-");
    
    Rcv_CommInterface = 0;
	Rcv_CaliType = 0;
	Rcv_CaliStatus = 0;
	UI_Init_Flag = 0;
	CaliTypeFlag = 0;
	timer_counter = 0;
    memcpy(Rcv_CaliTypePointStr, "", sizeof(char) * 20);
    
    Start_Cali_thread();
    
    gtk_label_set_markup(label_R, "<span font='24' foreground='#8B4513'><b>Running</b></span>"); //just set text with some style
}
