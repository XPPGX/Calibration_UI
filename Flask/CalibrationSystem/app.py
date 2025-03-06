from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
from datetime import datetime
import time
import ctypes
from threading import Thread
from collections import defaultdict
import os
import json
import math
import random
##################################################
# Global Variables
##################################################
comm_type_cases = {
    1 : "CANBUS",
    2 : "MODBUS",
    #Default: "-",
}

cali_type_cases = {
    0x80 : "ACV_DC_OFFSET_3相",
    0x40 : "ACV_DC_OFFSET_1相",
    0x20 : "ACI_DC_OFFSET_3相",
    0x10 : "ACI_DC_OFFSET_1相",
    0x08 : "ACI_3相",
    0x04 : "ACI_1相",
    0x01 : "DC",
}

adjust_mode_cases = {
    0 : "微調",
    1 : "粗調",
}


cali_status_cases = defaultdict(lambda: "Running...",  ## Suitable for a large number of default cases
    {
        1 : "FINISH",
        2 : "Not Complete",
        3 : "Device FAIL",
        4 : "DUT FAIL",
        5 : "Cali point FAIL",
        6 : "Peripheral FAIL"
    }
)

CurrentFolder_str = os.path.dirname(__file__)

UI_STATE_SERVER_RESET = 0
UI_STAGE_POLLING = 1                        # Get DUT's {ModelName, Communication Interface}
UI_STAGE_SEND_POINT = 2                     # Send information of current calibration point to Cali_thread
UI_STAGE_READ_FIXED_CALI_POINT_INFO = 3     # Read and show fixed information of current calibration point
# Check the finish that is 
# 1. "Not Completed(0x0000)", which means partial complete
# 2. "Success(0x0001)", which means all cali_point are done
UI_STAGE_READ_DYANAMIC_VALUES = 4           # Read the values that changed dynamically from Cali_thread
UI_STAGE_CHECK_FINISH = 5


UI_stage = UI_STATE_SERVER_RESET
ModelName_str = "-"
CommInterface_str   = "-"
CaliType_str    = "-"
CaliPoint_str       = "-"
AdjustMode_str      = "-"
CaliStatus_str      = "-"

current_cali_point_count = 0
current_cali_point_step_cmd = ""
change_to_next_cali_point_flag = 0


TargetEquipment_str = "-"
High_limit_str      = "-"
Low_limit_str       = "-"
TargetEquipment_Value_str = "-"
EQ_ValueColor       = 0


#Script
ScriptName_str = ""
CurrentScript_dict = {} # Data is from the script file that we select
ScriptFolderPath_str = "./scripts"
ScriptFilePath_str = ""
Cali_point_step_cmd_mapping = {} # Data is from the "CurrentScript_dict". Its mapping is like "0x0001" : "ACI_DC_Offset_單相"
LAST_USED_SCRIPT_FILE_PATH = "./Last_used_script.json"

#Devices_setting
Device_info_to_Web_dict = {}
DEVICE_JSON_FILE_PATH = "./DeviceConfig/DeviceConfig.json"
FINE_SETTING_JSON_FILE_PATH = "./DeviceConfig/Fine_setting.json"
Devices_setting = {} #For current linked devices

#Password
Password = set()
Password.add("admin")
pass_or_not_flag = 0
token_second_counter = 0
TOKEN_ALIVE_TIME = 10 # in second

app = Flask(__name__)
c_lib_Cali = ctypes.CDLL('./Cali_Code/Cali.so')
#c_lib_Search_Device = ctypes.CDLL('./Clib/Cali_Code/Auto_Search_Device.so')

DEBUG_MODE = 1 #DEBUG_MODE == 1, means it can run without DUT ; DEBUG_MODE = 0 means it needs to connect DUT to run
DEBUG_DEVICE = 1 #DEBUG_DEVICE == 1, means it can run without any device ; DEBUG_DEVICE == 0, means it needs to connect device to run
Debug_Enter_pressed = 0

##################################################
# C_lib parameters declaration
##################################################
c_lib_Cali.Get_Machine_Name.argtypes                        = []
c_lib_Cali.Get_Machine_Name.restype                         = ctypes.c_char_p

c_lib_Cali.Get_Communication_Type.argtypes                  = []
c_lib_Cali.Get_Communication_Type.restype                   = ctypes.c_uint8

c_lib_Cali.Get_Keyboard_Adjustment.argtypes                 = []
c_lib_Cali.Get_Keyboard_Adjustment.restype                  = ctypes.c_uint8

c_lib_Cali.Get_Calibration_Status.argtypes                  = []
c_lib_Cali.Get_Calibration_Status.restype                   = ctypes.c_uint8

c_lib_Cali.Check_UI_Reset_Cali.argtypes                     = [ctypes.c_uint16]
c_lib_Cali.Check_UI_Reset_Cali.restype                      = None

c_lib_Cali.Check_UI_Set_Cali_Point_Flag.argtypes            = [ctypes.c_uint16]
c_lib_Cali.Check_UI_Set_Cali_Point_Flag.restype             = None

c_lib_Cali.Check_UI_Set_Cali_Point.argtypes                 = [ctypes.c_uint16]
c_lib_Cali.Check_UI_Set_Cali_Point.restype                  = None

c_lib_Cali.Check_UI_Set_Cali_Point_Scaling_Factor.argtypes  = [ctypes.c_char_p]
c_lib_Cali.Check_UI_Set_Cali_Point_Scaling_Factor.restype   = None

c_lib_Cali.Check_UI_Set_Cali_Point_USB_Port.argtypes        = [ctypes.c_char_p]
c_lib_Cali.Check_UI_Set_Cali_Point_USB_Port.restype         = None

c_lib_Cali.Get_PSU_Cali_Point_High_Limit.argtypes           = []
c_lib_Cali.Get_PSU_Cali_Point_High_Limit.restype            = ctypes.c_float

c_lib_Cali.Get_PSU_Cali_Point_Low_Limit.argtypes            = []
c_lib_Cali.Get_PSU_Cali_Point_Low_Limit.restype             = ctypes.c_float

c_lib_Cali.Get_Device_Measured_Value.argtypes               = []
c_lib_Cali.Get_Device_Measured_Value.restype                = ctypes.c_float

c_lib_Cali.Get_Calibration_Point_Complete.argtypes          = []
c_lib_Cali.Get_Calibration_Point_Complete.restype           = ctypes.c_uint8

c_lib_Cali.Get_Valid_Measured_Value.argtypes                = []
c_lib_Cali.Get_Valid_Measured_Value.restype                 = ctypes.c_uint8

c_lib_Cali.Check_UI_Set_Cali_Point_Target.argtypes          = [ctypes.c_char_p]
c_lib_Cali.Check_UI_Set_Cali_Point_Target.restype           = None

c_lib_Cali.Check_Calibration_Point_Complete.argtypes          = [ctypes.c_uint8]
c_lib_Cali.Check_Calibration_Point_Complete.restype           = None

c_lib_Cali.Start_Cali_thread.argtypes                       = []
c_lib_Cali.Start_Cali_thread.restype                        = None

c_lib_Cali.Stop_Cali_thread.argtypes                        = []
c_lib_Cali.Stop_Cali_thread.restype                         = None

c_lib_Cali.scan_usb_devices.argtypes               = []
c_lib_Cali.scan_usb_devices.restype                = None

c_lib_Cali.Get_Device_information.argtypes         = [ctypes.c_int]
c_lib_Cali.Get_Device_information.restype          = ctypes.c_char_p
##################################################
# Server Functions
##################################################
def server_timers():
    global UI_stage
    global ModelName_str
    global CommInterface_str
    global CaliType_str
    global CaliPoint_str
    global AdjustMode_str
    global CaliStatus_str
    global ScriptName_str

    global High_limit_str
    global Low_limit_str
    global TargetEquipment_Value_str

    global CurrentScript_dict
    global Cali_point_step_cmd_mapping
    global Devices_setting
    
    global current_cali_point_count
    global current_cali_point_step_cmd
    global change_to_next_cali_point_flag

    global EQ_ValueColor
    
    single_task = {} #dictionary
    tasks_list = []

    old_CaliType_str = ""
    old_CaliPoint_str = ""

    #Initialize the global variables
    if(UI_stage == UI_STATE_SERVER_RESET):
        ModelName_str = "-"
        CommInterface_str = "-"
        CaliPoint_str = "-"
        AdjustMode_str = "微調"
        CaliStatus_str = "-"
        ScriptName_str = ""
        current_cali_point_count = 0
        current_cali_point_step_cmd = ""

        UI_stage = UI_STAGE_POLLING
        EQ_ValueColor = 0
        
    while True:
        if(UI_stage == UI_STAGE_POLLING):
            print("========== UI_STAGE_POLLING")
            # Get DUT ModelName, CommInterface
            ModelName_str       = getModelName()
            CommInterface_str   = getCommInterface()
            AdjustMode_str      = getAdjustMode()
            CaliStatus_str      = getCaliStatus()
            
            print(f"ModelName = {ModelName_str}")
            print(f'CommInterface_str = {CommInterface_str}')
            print(f'CaliStatus_str = {CaliStatus_str}')
            print(f'AdjustMode_str = {AdjustMode_str}')

            if(CommInterface_str != "-" and len(ModelName_str) > 6):
                # GetScript()
                
                UI_stage = UI_STAGE_SEND_POINT

            if(CaliStatus_str == "Device FAIL" or CaliStatus_str == "DUT FAIL" 
            or CaliStatus_str == "Cali point FAIL" or CaliStatus_str == "Peripheral FAIL"):
                UI_stage = UI_STATE_SERVER_RESET
                c_lib_Cali.Stop_Cali_thread()
                break

        elif(UI_stage == UI_STAGE_SEND_POINT):
            print("========== UI_STAGE_SEND_POINT")
            current_cali_point_step_cmd = CurrentScript_dict["Order"][current_cali_point_count]

            if(current_cali_point_step_cmd != "0xffff"): #Get this point's information
                current_cali_point_str = Cali_point_step_cmd_mapping[current_cali_point_step_cmd] #Get Cali_point
                cali_point_object = CurrentScript_dict["CaliPoints_Info"][current_cali_point_str]
                
                current_scaling_factor_str = cali_point_object["scaling_factor"] #Get Scaling_factor
                current_target_equipment_name = cali_point_object["Target_Equipment"]["Equipment_Name"]
                print("-------Devices setting\n")
                print(Devices_setting)
                current_usb_port_str = Devices_setting[current_target_equipment_name]["USB_Port"] #Get USB_port
                print(f'cali_point = {current_cali_point_str}, usb_port = {current_usb_port_str}')
                
                current_cali_point_target = ""
                print("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~")
                if(Devices_setting[current_target_equipment_name]["ModelName"] == "Chroma,51101-8"):
                    print("Chroma,51101-8")
                    current_cali_point_target = GetCaliPointTarget(cali_point_object, "Chroma 51101-8")
                    # tmp_channel = cali_point_object["Target_Equipment"]["Value_Type"]
                    # tmp_channel = tmp_channel[2:]
                    # print(tmp_channel)
                    # for channel in Devices_setting["Fine-setting"]["Chroma 51101-8"]:
                    #     if(channel["Ch"] == tmp_channel):
                    #         current_cali_point_target = tmp_channel + "," + channel["Value_Type"] + "," + channel["Shunt_Value"] #Get cali_point_target
                    #         print(f'current_target = {current_cali_point_target}')
                    #         break
                elif(Devices_setting[current_target_equipment_name]["ModelName"] == "GWInstek,GDM8342"):
                    print("GWInstek,GDM8342")
                    current_cali_point_target = GetCaliPointTarget(cali_point_object, "GWInstek,GDM8342")
                    # tmp_channel = cali_point_object["Target_Equipment"]["Value_Type"]
                    # tmp_channel = tmp_channel[2:]
                    # print(tmp_channel)
                    # for channel in Devices_setting["Fine-setting"]["GWInstek,GDM8342"]:
                    #     if(channel["Ch"] == tmp_channel):
                    #         current_cali_point_target = tmp_channel + "," + channel["Value_Type"] + "," + channel["Shunt_Value"] #Get cali_point_target
                    #         print(f'current_target = {current_cali_point_target}')
                    #         break
                else:
                    current_cali_point_target = cali_point_object["Target_Equipment"]["Value_Type"] #Get cali_point_target
                    print(f'current_target = {current_cali_point_target}')

                SendCaliPointInfo(current_cali_point_step_cmd, current_scaling_factor_str, current_cali_point_target, current_usb_port_str)
                
                UI_stage = UI_STAGE_READ_FIXED_CALI_POINT_INFO
                
                #DEBUG (Move cali_point to next index)
                # current_cali_point_count = current_cali_point_count + 1
            else:
                SendCaliPointInfo(current_cali_point_step_cmd, '', '', '')
                UI_stage = UI_STAGE_CHECK_FINISH
                
        elif(UI_stage == UI_STAGE_READ_FIXED_CALI_POINT_INFO):
            print("========== UI_STAGE_READ_FIXED_CALI_POINT_INFO")
            current_cali_point_str      = Cali_point_step_cmd_mapping[current_cali_point_step_cmd] #Get Cali_point
            cali_point_object           = CurrentScript_dict["CaliPoints_Info"][current_cali_point_str]
            #current_scaling_factor_str  = cali_point_object["scaling_factor"] #Get Scaling_factor
            #if(current_scaling_factor_str == "NO"):
            #    High_limit_str  = "None"
            #    Low_limit_str   = "None"
                
            #    UI_stage = UI_STAGE_READ_DYANAMIC_VALUES
            #else:
            tmp_Upper_Bound = getUpperBound()
            tmp_Lower_Bound = getLowerBound()
            if math.isclose(tmp_Upper_Bound, 0.0, abs_tol=1e-6) or math.isclose(tmp_Lower_Bound, 0.0, abs_tol=1e-6):
                # Any one of the bound didn't get the value from Cali_Thread
                High_limit_str  = "-"
                Low_limit_str   = "-"
            else:
                # Both of the bounds got the value from Cali_Thread
                High_limit_str  = f"{tmp_Upper_Bound:.3f}"
                Low_limit_str   = f"{tmp_Lower_Bound:.3f}"
                UI_stage = UI_STAGE_READ_DYANAMIC_VALUES

            CaliPoint_str = current_cali_point_str
            TargetEquipment_str = CurrentScript_dict["CaliPoints_Info"][CaliPoint_str]["Target_Equipment"]["Equipment_Name"]
            
            #print("test")
            print(f'{CaliPoint_str},{TargetEquipment_str},{Low_limit_str},{High_limit_str}')
            
            CaliStatus_str = getCaliStatus()
            
            if(CaliStatus_str == "Device FAIL" or CaliStatus_str == "DUT FAIL" 
            or CaliStatus_str == "Cali point FAIL" or CaliStatus_str == "Peripheral FAIL"):
                UI_stage = UI_STATE_SERVER_RESET
                c_lib_Cali.Stop_Cali_thread()
                break

        elif(UI_stage == UI_STAGE_READ_DYANAMIC_VALUES):
            print("========== UI_STAGE_READ_DYANAMIC_VALUES")
            AdjustMode_str = getAdjustMode()
            TargetEquipment_Value = getDeviceMeasureValue()
            TargetEquipment_Value_str = f"{TargetEquipment_Value:.3f}"
            CaliStatus_str = getCaliStatus()
            CaliPointComplete_uint8 = getCalibrationPointComplete()
            EQ_ValueColor = getTargetEQ_ValueColor()
            
            print(f'{AdjustMode_str}, {TargetEquipment_Value_str}, {CaliStatus_str}')
            if(CaliPointComplete_uint8 == 1):
                c_lib_Cali.Check_Calibration_Point_Complete(0)
                current_cali_point_count = current_cali_point_count + 1
                change_to_next_cali_point_flag = 1
                UI_stage = UI_STAGE_SEND_POINT
            
            if(CaliStatus_str == "Device FAIL" or CaliStatus_str == "DUT FAIL" 
            or CaliStatus_str == "Cali point FAIL" or CaliStatus_str == "Peripheral FAIL"):
                UI_stage = UI_STATE_SERVER_RESET
                c_lib_Cali.Stop_Cali_thread()
                break

        elif(UI_stage == UI_STAGE_CHECK_FINISH):
            print("========== UI_STAGE_CHECK_FINISH")
            CaliStatus_str = getCaliStatus()
            if(CaliStatus_str == "FINISH" or CaliStatus_str == "Not Complete" 
            or CaliStatus_str == "Device FAIL" or CaliStatus_str == "DUT FAIL" 
            or CaliStatus_str == "Cali point FAIL" or CaliStatus_str == "Peripheral FAIL"):
                c_lib_Cali.Stop_Cali_thread()
                print("Thread : Cali terminated.\n")
                print("Thread : ServerTimer Terminated.\n")
                UI_stage = UI_STATE_SERVER_RESET # Waiting for pressing button in next time
                break
        
        time.sleep(0.1)

def GetScript():
    global CurrentFolder_str
    global ScriptFolderPath_str
    global ScriptFilePath_str
    global CurrentScript_dict
    global ScriptName_str
    global ModelName_str
    
    global Cali_point_step_cmd_mapping

    # Get the part of ModelName_str before the last "-"
    ScriptName_str, _, _ = ModelName_str.rpartition("-")
    ScriptName_str = ScriptName_str + ".json"
    print("ScriptName_str : " + ScriptName_str)

    ScriptFilePath_str = os.path.join(ScriptFolderPath_str, ScriptName_str)
    print(f"ScriptFilePath_str = '{ScriptFilePath_str}'")

    try:
        with open(ScriptFilePath_str, "r") as script:
            CurrentScript_dict = json.load(script)
            print(CurrentScript_dict)
            json_str = json.dumps(CurrentScript_dict, indent=4, ensure_ascii=False)
            print(json_str)
            
            for cali_point_str in CurrentScript_dict["CaliPoints_Info"]:
                tmp_step_cmd = CurrentScript_dict["CaliPoints_Info"][cali_point_str]["step_cmd"]
                Cali_point_step_cmd_mapping[tmp_step_cmd] = cali_point_str    
            
    except FileNotFoundError:
        print(f"[Error] File '{ScriptFilePath_str}' is not exist")
    except json.JSONDecodeError:
        print(f"[Error] File '{ScriptFilePath_str}' Invalid JSON format")

def Init_getDeviceSetting_from_Local_File():
    global Devices_setting
    Devices_setting = {}

    #Load local deviceConfig json file to "Devices_setting"
    try:
        with open(DEVICE_JSON_FILE_PATH, "r", encoding="utf-8") as file:
            devices_JSON = json.load(file)

    except FileNotFoundError:
        print("Error : JSON file not found")
    except json.JSONDecodeError:
        print("Error : Json format error")
    
    fine_setting_flag = 0
    for Mfr, Series in devices_JSON.items():
        for model in Series:
            if(model == "51101-8"):
                fine_setting_flag = 1
            for index in range(len(Series[model]["Identifier"])):
                tmp_EQ_type_str = Series[model]["EQ_TYPE"]
                tmp_SerialNum_str = Series[model]["SerialNumber"][index]
                tmp_CodeName_str = Series[model]["Identifier"][index]
                tmp_USB_Port_str = Series[model]["USB_Port"][index]
                print(f'({index}) : ModelName = {Mfr + "," + model}, EQ_TYPE = {tmp_EQ_type_str}, SeriNum = {tmp_SerialNum_str}, ID = {tmp_CodeName_str}, USB_Port = {tmp_USB_Port_str}')
                
                tmp_dict = {}
                tmp_dict["Type"] = tmp_EQ_type_str
                tmp_dict["ModelName"] = Mfr + "," + model
                tmp_dict["SerialNum"] = tmp_SerialNum_str
                tmp_dict["USB_Port"] = tmp_USB_Port_str
                
                Devices_setting[tmp_CodeName_str] = tmp_dict

    # Load Fine-setting from local file to "Device_setting"
    if(fine_setting_flag == 1):
        with open(FINE_SETTING_JSON_FILE_PATH, "r", encoding="utf-8") as file:
            fine_setting = json.load(file)
            json_str = json.dumps(fine_setting, indent=4)

        Devices_setting["Fine-setting"] = fine_setting

    print("[Result]")
    print(json.dumps(Devices_setting, indent=4))
    
def Init_getScript_from_Local_File():
    global CurrentScript_dict
    global Cali_point_step_cmd_mapping
    CurrentScript_dict = {}
    Cali_point_step_cmd_mapping = {}

    lastTime_ScriptName = ""
    script_file_path = ""
    try:
        # Get the script name we used in last time
        with open(LAST_USED_SCRIPT_FILE_PATH, "r", encoding="utf-8") as file:
            tmp_json = json.load(file)
            lastTime_ScriptName = tmp_json["ScriptName"]
            print(f'lastTime_ScriptName = {lastTime_ScriptName}')

        # Get the script content we used in last time
        script_file_path = os.path.join(ScriptFolderPath_str, lastTime_ScriptName)
        with open(script_file_path, "r", encoding="utf-8") as file:
            CurrentScript_dict = json.load(file)
            for cali_point_str in CurrentScript_dict["CaliPoints_Info"]:
                tmp_step_cmd = CurrentScript_dict["CaliPoints_Info"][cali_point_str]["step_cmd"]
                Cali_point_step_cmd_mapping[tmp_step_cmd] = cali_point_str

    except FileNotFoundError:
        print("Error : JSON file not found")
    except json.JSONDecodeError:
        print("Error : Json format error")

def Get_cmd_to_CaliPoint_mapping():
    global CurrentScript_dict
    global Cali_point_step_cmd_mapping

    if(CurrentScript_dict != {}):
        Cali_point_step_cmd_mapping = {}
        for cali_point_str in CurrentScript_dict["CaliPoints_Info"]:
            tmp_step_cmd = CurrentScript_dict["CaliPoints_Info"][cali_point_str]["step_cmd"]
            Cali_point_step_cmd_mapping[tmp_step_cmd] = cali_point_str

def reset_pass_or_not_flag_thread():
    global pass_or_not_flag
    global token_second_counter

    while (token_second_counter > 0):
        token_second_counter = token_second_counter - 1
        time.sleep(1) #sleep 300s

    pass_or_not_flag = 0

def getModelName():
    if(DEBUG_MODE == 0):
        ModelName_ptr = c_lib_Cali.Get_Machine_Name()
        ModelName_str = ModelName_ptr.decode("utf-8")
        return ModelName_str
    else:
        ModelName_str = "BIC-5K-48"
        return ModelName_str
    
def getCommInterface():
    if(DEBUG_MODE == 0):
        CommInterface_uint8 = c_lib_Cali.Get_Communication_Type()
        print(f'CommInterface_uint8 = {CommInterface_uint8}')
        time.sleep(5)
        CommInterface_str = comm_type_cases.get(CommInterface_uint8, "-") #"-" is default value
        return CommInterface_str
    else:
        CommInterface_str = "CANBUS"
        return CommInterface_str
    
def getAdjustMode():
    if(DEBUG_MODE == 0):
        AdjustMode_uint8 = c_lib_Cali.Get_Keyboard_Adjustment()
        AdjustMode_str = adjust_mode_cases[AdjustMode_uint8]
        return AdjustMode_str
    else:
        AdjustMode_str = adjust_mode_cases[0]
        return AdjustMode_str

def getCaliStatus():
    if(DEBUG_MODE == 0):
        CaliStatus_uint8 = c_lib_Cali.Get_Calibration_Status()
        CaliStatus_str = cali_status_cases[CaliStatus_uint8]
        return CaliStatus_str
    else:
        global UI_stage
        CaliStatus_str = cali_status_cases[0]
        
        if(UI_stage == UI_STAGE_CHECK_FINISH):
            CaliStatus_str = cali_status_cases[1]
        
        return CaliStatus_str
    
def getUpperBound():
    if(DEBUG_MODE == 0):
        return c_lib_Cali.Get_PSU_Cali_Point_High_Limit()
    else:
        return 100
    
def getLowerBound():
    if(DEBUG_MODE == 0):
        return c_lib_Cali.Get_PSU_Cali_Point_Low_Limit()
    else:
        return 50

def getDeviceMeasureValue():
    if(DEBUG_DEVICE == 0):
        return c_lib_Cali.Get_Device_Measured_Value()
    else:
        return random.uniform(1, 100)
        # return 77.777

def getCalibrationPointComplete():
   
    if(DEBUG_MODE == 0):
        return c_lib_Cali.Get_Calibration_Point_Complete()
    
    else:
        global Debug_Enter_pressed
        temp_enter_pressed = Debug_Enter_pressed

        if(Debug_Enter_pressed == 1):
            Debug_Enter_pressed = 0

        return temp_enter_pressed

def getTargetEQ_ValueColor():
    tmp_EQ_ValueColor = c_lib_Cali.Get_Valid_Measured_Value()
    return tmp_EQ_ValueColor 

def SendCaliPointInfo(_step_cmd, _scaling_factor, _target, _usb_port):
    
    step_cmd_hex = int(_step_cmd, 16)
    step_cmd_hex_C_format = ctypes.c_uint16(step_cmd_hex) #Get step_cmd_hex
    print(f'[DEBUG] : _step_cmd = {step_cmd_hex}')

    #Send the cali point information to Cali_thread
    c_lib_Cali.Check_UI_Set_Cali_Point(step_cmd_hex_C_format)
    c_lib_Cali.Check_UI_Set_Cali_Point_Scaling_Factor(str(_scaling_factor).encode("utf-8"))
    c_lib_Cali.Check_UI_Set_Cali_Point_USB_Port(str(_usb_port).encode("utf-8"))
    c_lib_Cali.Check_UI_Set_Cali_Point_Target(str(_target).encode("utf-8"))
    
    #Raise the Flag to tell Cali_thread that Info has been transmitted
    c_lib_Cali.Check_UI_Set_Cali_Point_Flag(1)

def GetCaliPointTarget(_cali_point_object, _device_name):
    current_cali_point_target = ""

    tmp_channel = _cali_point_object["Target_Equipment"]["Value_Type"]
    tmp_channel = tmp_channel[2:]
    print(tmp_channel)
    for channel in Devices_setting["Fine-setting"][_device_name]:
        if(channel["Ch"] == tmp_channel):
            current_cali_point_target = tmp_channel + "," + channel["Value_Type"] + "," + channel["Shunt_Value"] #Get cali_point_target
            print(f'current_target = {current_cali_point_target}')
            break
    return current_cali_point_target
##################################################
# Web page routes
##################################################
@app.route('/')
@app.route('/index')
def index():
    return render_template('index.html')

@app.route('/equipment_detect')
def equipment_detect():
    return render_template('equipment_detect.html')

@app.route('/Calibration_schedule')
def Calibration_schedule():
    return render_template('Calibration_schedule.html')

##################################################
# WebAPI
##################################################
@app.route('/api/START_CALI_PROCESS', methods=['POST'])
def handle_START_CALI_PROCESS():
    #Before pressing the start_button, the following data need to be ready

    global CurrentScript_dict
    global Cali_point_step_cmd_mapping
    cmd_length = len(CurrentScript_dict["Order"])
    print(f'cmd_length = {cmd_length}')

    if(CurrentScript_dict == {}):
        return jsonify({'OK_Flag' : 0}), 200
    if(Cali_point_step_cmd_mapping == {}):
        return jsonify({'OK_Flag' : 0}), 200
    if(cmd_length <= 1):
        return jsonify({'OK_Flag' : 0}), 200
    
    Rcv_json_object = request.get_json()
    Re_Cali_Flag = Rcv_json_object["Re_cali"]
    if(Re_Cali_Flag == True):
        #Send UI_flag is YES to Cali global var
        c_lib_Cali.Check_UI_Reset_Cali(1)
        print(Re_Cali_Flag)
    elif(Re_Cali_Flag == False):
        #Send UI_flag is NO to Cali global var
        c_lib_Cali.Check_UI_Reset_Cali(2)
        print(Re_Cali_Flag)

    if(DEBUG_MODE == 0):
        c_lib_Cali.Start_Cali_thread()

    thread = Thread(target=server_timers)
    thread.daemon = True
    thread.start()
    return jsonify({'OK_Flag' : 1}), 200

@app.route('/api/stage_query', methods=['GET'])
def handle_stage_query():
    global UI_stage
    data = jsonify({'WebAPI_Stage' : UI_stage})
    return data

@app.route('/api/update_ui_1st_stage', methods=['GET'])
def handle_update_ui_1st_stage():
    global ModelName_str
    global CommInterface_str
    global AdjustMode_str
    global CaliStatus_str

    data = jsonify({'WebAPI_ModelName'      : f'{ModelName_str}',
                    'WebAPI_CommInterface'  : f'{CommInterface_str}',
                    'WebAPI_AdjustMode'     : f'{AdjustMode_str}',
                    'WebAPI_CaliStatus'     : f'{CaliStatus_str}'})
    return data

@app.route('/api/update_ui_2nd_stage', methods=['GET'])
def handle_update_ui_2nd_stage():
    global CaliPoint_str
    global High_limit_str
    global Low_limit_str
    global CaliStatus_str

    tmp_done = 0
    global CurrentScript_dict
    
    #tmp_scaling_factor_str = CurrentScript_dict["CaliPoints_Info"][CaliPoint_str]["scaling_factor"]
    print(f"\t\t 2nd_stage : Cali_point = {CaliPoint_str}")

    tmp_equipment_name_str = CurrentScript_dict["CaliPoints_Info"][CaliPoint_str]["Target_Equipment"]["Equipment_Name"]
    tmp_step_cmd_str = CurrentScript_dict["CaliPoints_Info"][CaliPoint_str]["step_cmd"]
    tmp_step_cmd_int = int(tmp_step_cmd_str, 16)
    tmp_CaliPoint_str = "P{}. {}".format(str(tmp_step_cmd_int), CaliPoint_str)
    
    tmp_unit = CurrentScript_dict["CaliPoints_Info"][CaliPoint_str]["unit"]
    
    tmp_ValueType_str = CurrentScript_dict["CaliPoints_Info"][CaliPoint_str]["Target_Equipment"]["Value_Type"]
    
        
    #if(tmp_scaling_factor_str == "NO"):
    #    #For current cali point, there is no need of scaling_facotr
    #    tmp_done = 1
    #else:
        #For current cali point, the scaling_factor is needed
    if(High_limit_str != "-" and Low_limit_str != "-"):
        tmp_done = 1
    
    

    data = jsonify({'WebAPI_CaliPoint'      : f'{tmp_CaliPoint_str}',
                    'WebAPI_Equipment_Name' : f'{tmp_equipment_name_str}',
                    'WebAPI_ValueType'      : f'{tmp_ValueType_str}',
                    'WebAPI_Unit'           : f'{tmp_unit}',
                    'WebAPI_High_limit'     : f'{High_limit_str}',
                    'WebAPI_Low_limit'      : f'{Low_limit_str}',
                    'WebAPI_CaliStatus'     : f'{CaliStatus_str}',
                    'done'                  : tmp_done})
    return data

@app.route('/api/update_ui_3rd_stage', methods=['GET'])
def handle_update_ui_3rd_stage():
    global AdjustMode_str
    global CaliStatus_str
    global TargetEquipment_Value_str
    global change_to_next_cali_point_flag
    global EQ_ValueColor
    
    global CurrentScript_dict
    global CaliPoint_str
    tmp_unit = CurrentScript_dict["CaliPoints_Info"][CaliPoint_str]["unit"]
    

    data = jsonify({'WebAPI_AdjustMode' : f'{AdjustMode_str}',
                    'WebAPI_CaliStatus' : f'{CaliStatus_str}',
                    'WebAPI_unit'       : f'{tmp_unit}',
                    'WebAPI_Target_EQ_value' : f'{TargetEquipment_Value_str}',
                    'WebAPI_ValueColor' : f'{EQ_ValueColor}',
                    'WebAPI_change_point_flag' : change_to_next_cali_point_flag})
    
    if(change_to_next_cali_point_flag == 1):
        change_to_next_cali_point_flag = 0

    return data

@app.route('/api/scan_usb_devices', methods=['POST'])
def handle_scan_usb_devices():
    global Device_info_to_Web_dict
    DeviceInfo_str_list = []

    #Real mode
    if(DEBUG_DEVICE == 0):
        thread = Thread(target = c_lib_Cali.scan_usb_devices)
        thread.daemon = True
        thread.start()
        thread.join(timeout=10)
    
    
        DeviceInfo_str = ""
        DeviceName_str = ""
    
        count = 0
        #Get all devices' information from the Cali procedure
        while(1):
            DeviceInfo_ptr = c_lib_Cali.Get_Device_information(count)
            DeviceInfo_str = DeviceInfo_ptr.decode("utf-8")
                
            if(DeviceInfo_str == "Invalid Index" or DeviceInfo_str == ""):
                break
            else:
                DeviceInfo_str_list.append(DeviceInfo_str)

            count = count + 1
    
    #Debug mode
    elif(DEBUG_DEVICE == 1):
        device_info_1 = "/dev/ttyUSB0,Chroma,51101-8"
        # device_info_2 = "/dev/USBTMC0,Chroma ATE,66202,662025001432,1.70"
        device_info_2 = "/dev/USBTMC0,Chroma ATE,66205,662025001432,1.70"
        device_info_3 = "/dev/ttyUSB1,GWInstek,GDM8342,GER817373,1.04"
        device_info_4 = "/dev/ttyUSB2,ATA1045"
        DeviceInfo_str_list.append(device_info_1)
        DeviceInfo_str_list.append(device_info_2)
        DeviceInfo_str_list.append(device_info_3)
        DeviceInfo_str_list.append(device_info_4)
        print(DeviceInfo_str_list)
    
    
    #Load local deviceConfig json file
    try:
        with open(DEVICE_JSON_FILE_PATH, "r", encoding="utf-8") as file:
            devices_JSON = json.load(file)
            print("======Content in DeviceConfig.json======")
            print(json.dumps(devices_JSON, ensure_ascii=False, indent=4))
    except FileNotFoundError:
        print("Error : JSON file not found")
    except json.JSONDecodeError:
        print("Error : Json format error")

    

    #Parse the devices' information
    Device_info_to_Web_dict = {}
    
    EQ_type = ""
    combine_name = ""
    for info in DeviceInfo_str_list:

        split_info_str_list = info.split(",")
        usb_port = split_info_str_list[0]

        mfr_name = ""
        model_name = ""
        serial_num = ""

        print("==split info_str")
        split_info_str_list = info.split(",")
        if(len(split_info_str_list) >= 2):
            mfr_name = split_info_str_list[1]
            combine_name = mfr_name
            print(mfr_name)
        if(len(split_info_str_list) >= 3):
            model_name = split_info_str_list[2]
            combine_name = mfr_name + "," + model_name
            print(model_name)
        if(len(split_info_str_list) >= 4):
            serial_num = split_info_str_list[3]
            print(serial_num)
        print(mfr_name + "," + model_name + "," + serial_num)

        EQ_type = devices_JSON[combine_name]["EQ_TYPE"]
        devices_JSON[combine_name]["SerialNumber"].append(serial_num)
        Device_info_to_Web_dict[combine_name] = {}
        Device_info_to_Web_dict[combine_name]["EQ_TYPE"] = EQ_type
        Device_info_to_Web_dict[combine_name]["Serial_Num"] = serial_num
        Device_info_to_Web_dict[combine_name]["USB_Port"] = usb_port
            
    print(Device_info_to_Web_dict)
    return jsonify(Device_info_to_Web_dict), 200
    
@app.route('/api/save_devices_setting', methods=['POST'])
def handle_save_devices_setting():
    global Devices_setting
    tmp_device_file_data = {}

    try:
        # Receive device data from UI in json format
        Devices_setting = request.get_json()
        
        # Reset device data which got from local json file
        with open(DEVICE_JSON_FILE_PATH, "r", encoding="utf-8") as file:
            tmp_device_file_data = json.load(file)
            for name, content in tmp_device_file_data.items():
                content["SerialNumber"] = []
                content["Identifier"] = []
                content["USB_Port"] = []
                # print("==0.0==")
                # print(content)

        print("==== file_content (After clear the data) : ====")
        print(json.dumps(tmp_device_file_data, indent=4))
        print("==== UI   content (also in server variable \"Devices_setting\"): ====")
        print(json.dumps(Devices_setting, indent=4))
        
        # Traverse the json data got from UI, and save the content to the json file
        for CodeName, Info in Devices_setting.items():
            if(CodeName != "Fine-setting"):
                tmp_ModelName_str = Info["ModelName"]
                tmp_USB_Port_str = Info["USB_Port"]
                tmp_SerialNum_str = Info["SerialNum"]

                tmp_device_file_data[tmp_ModelName_str]["Identifier"].append(CodeName)
                tmp_device_file_data[tmp_ModelName_str]["SerialNumber"].append(tmp_SerialNum_str)
                tmp_device_file_data[tmp_ModelName_str]["USB_Port"].append(tmp_USB_Port_str)

        with open(DEVICE_JSON_FILE_PATH, "w", encoding="utf-8") as file:
            json.dump(tmp_device_file_data, file, ensure_ascii=False, indent=4)



        # Save Fine-setting data
        with open(FINE_SETTING_JSON_FILE_PATH, "w", encoding="utf-8") as file:
            json.dump(Devices_setting["Fine-setting"], file, ensure_ascii=False, indent=4)
        
                  
        return jsonify({}), 200
    
    except Exception as e:
        print(e) ## Print error message in the terminal
        return jsonify({'error' : str(e)}), 400

@app.route('/api/get_devices_setting', methods=['GET'])
def handle_get_devices_setting():
    print("HELLO")
    json_str = json.dumps(Devices_setting, indent=4)
    print(json_str)
    return jsonify(Devices_setting), 200

#No use currently
@app.route('/api/get_non_saved_device_info', methods=['GET'])
def get_non_saved_device_info():
    json_str = json.dumps(Device_info_to_Web_dict, indent=4)
    print(json_str)
    return jsonify(Device_info_to_Web_dict), 200

@app.route('/api/get_script_file_names', methods=['GET'])
def handle_get_script_file_names():
    files = os.listdir(ScriptFolderPath_str)
    print(files)
    return jsonify({"files" : files})

@app.route('/api/get_script_file_content', methods=['POST'])
def handle_get_script_file_content():
    # After selecting the file on WebUI, the "CurrentScript_dict" and "Cali_point_step_cmd_mapping"
    # will be filled with the data from script file, 


    global CurrentScript_dict
    global Cali_point_step_cmd_mapping
    global Devices_setting

    CurrentScript_dict = {}
    Cali_point_step_cmd_mapping = {}

    Device_Script_NotMatch_Flag = 0
    tmp_CurrentScript_dict = {}
    tmp_JSON_to_UI = {}
    try:
        # Get the data from UI via WebAPI
        Rcv_Json_data = request.get_json()

        script_File_Name = Rcv_Json_data.get('name', "File_Not_Found")
        print(script_File_Name)
        
        script_File_Path = os.path.join(ScriptFolderPath_str, script_File_Name)
        print(script_File_Path)

        # Open the script json file
        with open(script_File_Path, 'r') as file:
            # Assign "tmp_CurrentScript_dict" the json content
            tmp_CurrentScript_dict = json.load(file)
            # print(json.dumps(tmp_CurrentScript_dict, indent=4, ensure_ascii=False))
            # print(json.dumps(Devices_setting, indent=4, ensure_ascii=False))
            
            # check if the EQ_Name in script is also in Device_setting
            tmp_CaliPoint_Infos = tmp_CurrentScript_dict["CaliPoints_Info"]            
            for cali_point_str, obj in tmp_CaliPoint_Infos.items():
                tmp_info = tmp_CaliPoint_Infos[cali_point_str]

                if(tmp_info["Target_Equipment"]["Equipment_Name"] is not None):
                    tmp_EQ_NAME = tmp_info["Target_Equipment"]["Equipment_Name"]
                    if(tmp_EQ_NAME == ""):
                        continue

                    if(tmp_EQ_NAME not in Devices_setting):
                        print(f'tmp_EQ_NAME = {tmp_EQ_NAME}')
                        print("Device and Script are not match")
                        Device_Script_NotMatch_Flag = 1
                        break

            if(Device_Script_NotMatch_Flag == 0):
                CurrentScript_dict = tmp_CurrentScript_dict
                # Assign "Cali_point_step_cmd_mapping" the order in "tmp_CurrentScript_dict" 
                Get_cmd_to_CaliPoint_mapping()
                print(Cali_point_step_cmd_mapping)

            else:
                CurrentScript_dict = {}
                Cali_point_step_cmd_mapping = {}
                print("Clear the Script and Mapping")


        # Get the script file name in last time 
        with open(LAST_USED_SCRIPT_FILE_PATH, "r", encoding="utf-8") as f:
            tmp_json = json.load(f)

        tmp_json["ScriptName"] = script_File_Name

        with open(LAST_USED_SCRIPT_FILE_PATH, "w", encoding="utf-8") as f:
            json.dump(tmp_json, f, ensure_ascii=False, indent=4)

        # return the API request with tmp_CurrentScript_dict. 200 means "OK" in API protocol
        tmp_JSON_to_UI["CaliInfos"] = tmp_CurrentScript_dict
        tmp_JSON_to_UI["stepMapping"] = Cali_point_step_cmd_mapping
        return tmp_JSON_to_UI, 200
        
    except Exception as e:
        return jsonify({'error' : str(e)}), 400 #400 means "ERROR" in API protocol's error code
    
@app.route('/api/save_modified_script', methods=['POST'])
def handle_save_modified_script():
    global CurrentScript_dict
    global Cali_point_step_cmd_mapping
    if request.is_json:
        Rcv_Json_data = request.get_json()
        # Get script_File_Name
        script_File_Name = Rcv_Json_data.get('script_File_Name', "File_Not_Found")
        print(script_File_Name)
        
        #Get script_File_Path
        script_File_Path = os.path.join(ScriptFolderPath_str, script_File_Name)
        print(script_File_Path)

        with open(script_File_Path, "w", encoding="utf-8") as f:
            json.dump(Rcv_Json_data, f, indent=4, ensure_ascii=False)
        
        json_str = json.dumps(Rcv_Json_data, indent=4, ensure_ascii=False)
        print(json_str)
        
        CurrentScript_dict = {}
        Cali_point_step_cmd_mapping = {}

        CurrentScript_dict = Rcv_Json_data
        Get_cmd_to_CaliPoint_mapping()

        return jsonify({"message" : "JSON received", "data": Rcv_Json_data}), 200

@app.route('/api/init_load_cali_points', methods=['GET'])
def handle_init_load_cali_points():
    tmp_json_obj = {}
    if(CurrentScript_dict != {} and Cali_point_step_cmd_mapping != {}):
        tmp_json_obj["CaliPoint"] = CurrentScript_dict
        tmp_json_obj["step_cmd_mapping"] = Cali_point_step_cmd_mapping
    return jsonify(tmp_json_obj), 200

@app.route('/api/init_load_task_list', methods=['GET'])
def handle_init_load_task_list():
    tmp_json_obj = {}
    tmp_json_obj["Script_content"] = CurrentScript_dict
    tmp_json_obj["cmd_mapping"] = Cali_point_step_cmd_mapping
    return jsonify(tmp_json_obj), 200

@app.route('/api/check_passwd', methods=['POST'])
def handle_check_passwd():
    global Password
    global pass_or_not_flag
    if request.is_json:
        Json_obj = request.get_json()
        Rcv_passwd = Json_obj["passwd"]
        print(Rcv_passwd)
        if(Rcv_passwd in Password):
            pass_or_not_flag = 1
        return jsonify({"Pass_Flag" : pass_or_not_flag}), 200
    
    else:
        return jsonify(), 400

@app.route('/api/check_pass_or_not_flag', methods=['GET'])
def handle_check_pass_or_not_flag():
    global pass_or_not_flag
    return jsonify({"Pass_Flag" : pass_or_not_flag}), 200

@app.route('/api/token_counter_reset', methods=['POST'])
def handle_token_counter_reset():
    global token_second_counter

    if(token_second_counter > 0):
        token_second_counter = TOKEN_ALIVE_TIME
    else:
        token_second_counter = TOKEN_ALIVE_TIME
        thread = Thread(target=reset_pass_or_not_flag_thread)
        thread.daemon = True
        thread.start()
    
    return jsonify({}), 200
##################################################
# debug
##################################################
@app.route('/api/testing', methods=['GET'])
def handle_testing():
    json_Data = {}
    json_Data["msg"] = "hello"
    print(json_Data)
    return jsonify(json_Data), 200

@app.route('/api/Mock_enter_pressed', methods=['POST'])
def handle_Mock_enter_pressed():
    global Debug_Enter_pressed
    Rcv_Json_data = request.get_json()
    Debug_Enter_pressed = Rcv_Json_data["pressed"]
    print(f'Debug_Enter_pressed = {Debug_Enter_pressed}')
    return jsonify({}), 200

def Mock_equipment_value():
    return 24.86

##################################################
# main
##################################################
if __name__ == '__main__':
    # ~ Init_getDeviceSetting_from_Local_File()
    Init_getScript_from_Local_File()
    app.run(debug=True)
    # print("admin" in Password)
