#include "Cali.h"
#include "Auto_Search_Device.h"
#include "Modbus.h"

/* Parameter -----------------------------------------------*/
uint8_t Command_Flag = 0;

char UI_TestCommand[MAX_STRING_LENGTH] = {0};
char UI_Para1[MAX_STRING_LENGTH] = {0};
char UI_Para2[MAX_STRING_LENGTH] = {0};
char UI_Para3[MAX_STRING_LENGTH] = {0};
char UI_Para4[MAX_STRING_LENGTH] = {0};
char UI_Para5[MAX_STRING_LENGTH] = {0};
char UI_RangeMax[MAX_STRING_LENGTH] = {0};
char UI_RangeMin[MAX_STRING_LENGTH] = {0};

/* function -----------------------------------------------*/
void Set_Command_Process(void);
void Read_Command_Process(void);

//UI Set
void Check_UI_TestCommand(const char *TestCommand);
void Check_UI_Para1(const char *Para1);
void Check_UI_Para2(const char *Para2);
void Check_UI_Para3(const char *Para3);
void Check_UI_Para4(const char *Para4);
void Check_UI_Para5(const char *Para5);
void Check_UI_RangeMax(const char *RangeMax);
void Check_UI_RangeMin(const char *RangeMin);

//UI Get
uint8_t Get_Command_Flag(void);


void Set_Command_Process(void) {
    char TI_Step_Command[MAX_STRING_LENGTH] = "";
    Command_Flag = 1;

    //ACS
    if (strcmp(UI_TestCommand, "SetACS_Vout") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_ACS_SET_VOLT, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "SetACS_Frequency") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_ACS_SET_FREQ, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "SetACS_ON") == 0) {
        SCPI_Write_Process(UI_Para1, SCPI_SOURCE_ON);
    }
    else if (strcmp(UI_TestCommand, "SetACS_OFF") == 0) {
        SCPI_Write_Process(UI_Para1, SCPI_SOURCE_OFF);
    }
    //DCL
    else if (strcmp(UI_TestCommand, "SetDCL_CCIout_L1") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_CURR_STAT_L1, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "SetDCL_CCIout_L2") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_CURR_STAT_L2, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "SetDCL_CVVout_L1") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_VOLT_STAT_L1, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "SetDCL_CVVout_L2") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_VOLT_STAT_L2, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "SetDCL_CV_Ilimit") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_VOLT_STAT_ILIMIT, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "SetDCL_ON") == 0) {
        SCPI_Write_Process(UI_Para1, SCPI_LOAD_ON);
    }
    else if (strcmp(UI_TestCommand, "SetDCL_OFF") == 0) {
        SCPI_Write_Process(UI_Para1, SCPI_LOAD_OFF);
    }
    //MODBUS
    else if (strcmp(UI_TestCommand, "SetMOD_Command") == 0) {
        uint8_t mod_id = (uint8_t)strtoul(UI_Para1, NULL, 0);
        uint16_t start_addr = (uint16_t)strtoul(UI_Para2, NULL, 0);
        uint16_t mod_byte_value = (uint16_t)strtoul(UI_Para3, NULL, 0);
        TI_Modbus_TxProcess_Write(mod_id, start_addr, mod_byte_value);
    }
    //CANBUS
    else if (strcmp(UI_TestCommand, "SetCAN_Command") == 0) {
        uint32_t can_id = (uint32_t)strtoul(UI_Para1, NULL, 0);
        uint8_t can_dlc = (uint8_t)strtoul(UI_Para2, NULL, 0);
        uint16_t can_cmd = (uint16_t)strtoul(UI_Para3, NULL, 0);
        uint16_t can_byte_value = (uint16_t)strtoul(UI_Para4, NULL, 0);
        TI_Canbus_TxProcess_Write(can_id, can_dlc, can_cmd, can_byte_value);
    }
    //DWAM
    else if (strcmp(UI_TestCommand, "SetDWAM_CT_ON") == 0) {
        SCPI_Write_Process(UI_Para1, SCPI_SET_CT_ON);
    }
    else if (strcmp(UI_TestCommand, "SetDWAM_CT_OFF") == 0) {
        SCPI_Write_Process(UI_Para1, SCPI_SET_CT_OFF);
    }
    else if (strcmp(UI_TestCommand, "SetDWAM_CT_Ratio") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_SET_CT_RATIO, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    //DMM
    //Chroma 51101-8
    else if (strcmp(UI_TestCommand, "SetDMM_BinaryData(Chroma 51101)") == 0) {
        uint8_t Chroma51101_cmd = (uint8_t)strtoul(UI_Para2, NULL, 0);
        uint8_t Chroma51101_para1 = (uint8_t)strtoul(UI_Para3, NULL, 0);
        uint8_t Chroma51101_para2 = (uint8_t)strtoul(UI_Para4, NULL, 0);
        uint8_t Chroma51101_length = (uint8_t)strtoul(UI_Para5, NULL, 0);
        Chroma_51101_Write_Process(UI_Para1, Chroma51101_cmd, Chroma51101_para1, Chroma51101_para2, Chroma51101_length);
    }

    Command_Flag = 0;
}

void Read_Command_Process(void) {
    Command_Flag = 1;

    //DWAM
    //V
    if (strcmp(UI_TestCommand, "ReadDWAM_Vrms") == 0) {
        SCPI_Read_Process(UI_Para1, SCPI_VOLT_RMS);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_Vdc") == 0) {
        SCPI_Read_Process(UI_Para1, SCPI_VOLT_DC);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_Vpeak+") == 0) {
        SCPI_Read_Process(UI_Para1, SCPI_VOLT_PEAK_PLUS);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_Vpeak-") == 0) {
        SCPI_Read_Process(UI_Para1, SCPI_VOLT_PEAK_MINUS);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_VTHD") == 0) {
        SCPI_Read_Process(UI_Para1, SCPI_VOLT_THD);
    }
    //I
    else if (strcmp(UI_TestCommand, "ReadDWAM_Irms") == 0) {
        SCPI_Read_Process(UI_Para1, SCPI_CURR_RMS);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_Idc") == 0) {
        SCPI_Read_Process(UI_Para1, SCPI_CURR_DC);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_Ipeak+") == 0) {
        SCPI_Read_Process(UI_Para1, SCPI_CURR_PEAK_PLUS);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_Ipeak-") == 0) {
        SCPI_Read_Process(UI_Para1, SCPI_CURR_PEAK_MINUS);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_ITHD") == 0) {
        SCPI_Read_Process(UI_Para1, SCPI_CURR_THD);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_IINRUSH") == 0) {
        SCPI_Read_Process(UI_Para1, SCPI_CURR_INRUSH);
    }
    //W
    else if (strcmp(UI_TestCommand, "ReadDWAM_Preal") == 0) {
        SCPI_Read_Process(UI_Para1, SCPI_POWER_REAL);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_Pfactor") == 0) {
        SCPI_Read_Process(UI_Para1, SCPI_POWER_PFACTOR);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_Pdc") == 0) {
        SCPI_Read_Process(UI_Para1, SCPI_POWER_DC);
    }
    //F
    else if (strcmp(UI_TestCommand, "ReadDWAM_Frequency") == 0) {
        SCPI_Read_Process(UI_Para1, SCPI_FREQ);
    }
    //3Phi
    //V
    if (strcmp(UI_TestCommand, "ReadDWAM_Vrms_CH") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_VOLT_RMS_CH, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_Vdc_CH") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_VOLT_DC_CH, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_Vpeak+_CH") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_VOLT_PEAK_PLUS_CH, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_Vpeak-_CH") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_VOLT_PEAK_MINUS_CH, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_VTHD_CH") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_VOLT_THD_CH, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    //I
    else if (strcmp(UI_TestCommand, "ReadDWAM_Irms_CH") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_CURR_RMS_CH, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_Idc_CH") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_CURR_DC_CH, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_Ipeak+_CH") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_CURR_PEAK_PLUS_CH, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_Ipeak-_CH") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_CURR_PEAK_MINUS_CH, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_ITHD_CH") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_CURR_THD_CH, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_IInrush_CH") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_CURR_INRUSH_CH, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    //W
    else if (strcmp(UI_TestCommand, "ReadDWAM_Preal_CH") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_POWER_REAL_CH, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_Pfactor_CH") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_POWER_PFACTOR_CH, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    else if (strcmp(UI_TestCommand, "ReadDWAM_Pdc_CH") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_POWER_DC_CH, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }
    //F
    else if (strcmp(UI_TestCommand, "ReadDWAM_Frequency_CH") == 0) {
        snprintf(TI_Step_Command, sizeof(TI_Step_Command), "%s %s\n", SCPI_FREQ_CH, UI_Para2);
        SCPI_Write_Process(UI_Para1, TI_Step_Command);
    }

    //DMM
    //Chroma 51101-8
    else if (strcmp(UI_TestCommand, "ReadDMM_BinaryData(Chroma 51101)") == 0) {
        uint8_t Chroma51101_cmd = (uint8_t)strtoul(UI_Para2, NULL, 0);
        uint8_t Chroma51101_para1 = (uint8_t)strtoul(UI_Para3, NULL, 0);
        uint8_t Chroma51101_length = (uint8_t)strtoul(UI_Para4, NULL, 0);
        float Current_Shunt_Factor = atof(UI_Para5);
        Chroma_51101_Read_Process(UI_Para1, Chroma51101_cmd, Chroma51101_para1, Chroma51101_length);
    }

    Command_Flag = 0;   
}



void Check_UI_TestCommand(const char *TestCommand) {
    strncpy(UI_TestCommand, TestCommand, MAX_STRING_LENGTH - 1);
    UI_TestCommand[MAX_STRING_LENGTH - 1] = '\0';
}

void Check_UI_Para1(const char *Para1) {
    strncpy(UI_Para1, Para1, MAX_STRING_LENGTH - 1);
    UI_Para1[MAX_STRING_LENGTH - 1] = '\0';
}

void Check_UI_Para2(const char *Para2) {
    strncpy(UI_Para2, Para2, MAX_STRING_LENGTH - 1);
    UI_Para2[MAX_STRING_LENGTH - 1] = '\0';
}

void Check_UI_Para3(const char *Para3) {
    strncpy(UI_Para3, Para3, MAX_STRING_LENGTH - 1);
    UI_Para3[MAX_STRING_LENGTH - 1] = '\0';
}

void Check_UI_Para4(const char *Para4) {
    strncpy(UI_Para4, Para4, MAX_STRING_LENGTH - 1);
    UI_Para4[MAX_STRING_LENGTH - 1] = '\0';
}

void Check_UI_Para5(const char *Para5) {
    strncpy(UI_Para5, Para5, MAX_STRING_LENGTH - 1);
    UI_Para5[MAX_STRING_LENGTH - 1] = '\0';
}

void Check_UI_RangeMax(const char *RangeMax) {
    strncpy(UI_RangeMax, RangeMax, MAX_STRING_LENGTH - 1);
    UI_RangeMax[MAX_STRING_LENGTH - 1] = '\0';
}

void Check_UI_RangeMin(const char *RangeMin) {
    strncpy(UI_RangeMin, RangeMin, MAX_STRING_LENGTH - 1);
    UI_RangeMin[MAX_STRING_LENGTH - 1] = '\0';
}

uint8_t Get_Command_Flag(void) {
    return Command_Flag;
}