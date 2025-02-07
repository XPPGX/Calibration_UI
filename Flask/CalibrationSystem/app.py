from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
from datetime import datetime
import time
import ctypes
from threading import Thread
from collections import defaultdict
import os
import json
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
        3 : "FAIL"
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

#Script
ScriptName_str = ""
CurrentScript_dict = {} #For calibration
ScriptFolderPath_str = "./scripts"
ScriptFilePath_str = ""
Cali_point_step_cmd_mapping = {}

#Devices_setting
DEVICE_JSON_FILE_PATH = "./DeviceConfig/DeviceConfig.json"
Devices_setting = {} #For current linked devices

#Enter
enter_pressed = 0

app = Flask(__name__)
c_lib_Cali = ctypes.CDLL('./Clib/Cali_Code/Cali.so')
c_lib_Search_Device = ctypes.CDLL('./Clib/Cali_Code/Auto_Search_Device.so')
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

c_lib_Cali.Check_UI_Set_Cali_Point_Target.argtypes          = [ctypes.c_char_p]
c_lib_Cali.Check_UI_Set_Cali_Point_Target.restype           = None

c_lib_Cali.Start_Cali_thread.argtypes                       = []
c_lib_Cali.Start_Cali_thread.restype                        = None

c_lib_Cali.Stop_Cali_thread.argtypes                        = []
c_lib_Cali.Stop_Cali_thread.restype                         = None

c_lib_Search_Device.scan_usb_devices.argtypes               = []
c_lib_Search_Device.scan_usb_devices.restype                = None

c_lib_Search_Device.Get_Device_information.argtypes         = [ctypes.c_int]
c_lib_Search_Device.Get_Device_information.restype          = ctypes.c_char_p
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

    global enter_pressed
    
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
        TargetEquipment_Name_str = "-"
        current_cali_point_count = 0
        current_cali_point_step_cmd = ""
        enter_pressed = 0

        UI_stage = UI_STAGE_POLLING

    while True:
        if(UI_stage == UI_STAGE_POLLING) :
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
                GetScript()
                UI_stage = UI_STAGE_SEND_POINT

            if(CaliStatus_str == "FAIL"):
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
                if(Devices_setting[current_target_equipment_name]["ModelName"] == "Chroma 51101-8"):
                    tmp_channel = cali_point_object["Target_Equipment"]["Value_Type"]
                    tmp_channel = tmp_channel[2:]
                    print(tmp_channel)
                    for channel in Devices_setting["Fine-setting"]["Chroma 51101-8"]:
                        if(channel["Ch"] == tmp_channel):
                            current_cali_point_target = tmp_channel + "," + channel["Value_Type"] + "," + channel["Shunt_Value"] #Get cali_point_target
                            print(f'current_target = {current_cali_point_target}')
                            break
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
            current_scaling_factor_str  = cali_point_object["scaling_factor"] #Get Scaling_factor
            if(current_scaling_factor_str == "NO"):
                High_limit_str  = "None"
                Low_limit_str   = "None"
                
                UI_stage = UI_STAGE_READ_DYANAMIC_VALUES
            else:
                tmp_Upper_Bound = c_lib_Cali.Get_PSU_Cali_Point_High_Limit()
                tmp_Lower_Bound = c_lib_Cali.Get_PSU_Cali_Point_Low_Limit()
                if(tmp_Upper_Bound == 0 or tmp_Lower_Bound == 0):
                    # Any one of the bound didn't get the value from Cali_Thread
                    High_limit_str  = "-"
                    Low_limit_str   = "-"
                else:
                    # Both of the bounds got the value from Cali_Thread
                    High_limit_str  = str(tmp_Upper_Bound)
                    Low_limit_str   = str(tmp_Lower_Bound)
                    UI_stage = UI_STAGE_READ_DYANAMIC_VALUES

            CaliPoint_str = current_cali_point_str
            TargetEquipment_str = CurrentScript_dict["CaliPoints_Info"][CaliPoint_str]["Target_Equipment"]["Equipment_Name"]
            
            print("test")
            print(f'{CaliPoint_str},{TargetEquipment_str},{Low_limit_str},{High_limit_str}')
            
            CaliStatus_str = getCaliStatus()
            
            if(CaliStatus_str == "FAIL"):
                UI_stage = UI_STATE_SERVER_RESET
                c_lib_Cali.Stop_Cali_thread()
                break

        elif(UI_stage == UI_STAGE_READ_DYANAMIC_VALUES):
            print("========== UI_STAGE_READ_DYANAMIC_VALUES")
            AdjustMode_str = getAdjustMode()
            TargetEquipment_Value_str = str(Mock_equipment_value()) #Debug
            CaliStatus_str = getCaliStatus()
            
            print(f'{AdjustMode_str}, {TargetEquipment_Value_str}, {CaliStatus_str}')
            if(enter_pressed == 1):
                enter_pressed = 0
                current_cali_point_count = current_cali_point_count + 1
                change_to_next_cali_point_flag = 1
                UI_stage = UI_STAGE_SEND_POINT
            
            if(CaliStatus_str == "FAIL"):
                UI_stage = UI_STATE_SERVER_RESET
                c_lib_Cali.Stop_Cali_thread()
                break

        elif(UI_stage == UI_STAGE_CHECK_FINISH):
            print("========== UI_STAGE_CHECK_FINISH")
            if(CaliStatus_str == "FINISH" or CaliStatus_str == "Not Completed" or CaliStatus_str == "FAIL"):
                c_lib_Cali.Stop_Cali_thread()
                print("Thread : Cali terminated.\n")
                print("Thread : ServerTimer Terminated.\n")
                UI_stage = UI_STATE_SERVER_RESET # Waiting for pressing button in next time
                break
        
        time.sleep(0.1)

    #This thread's main loop
    # while True:
        
    #     if(UI_stage == 1):
    #         #Get ModelName string
    #         # ModelName_ptr = c_lib_Cali.Get_Machine_Name()
    #         # ModelName_str = ModelName_ptr.decode("utf-8")
    #         ModelName_str = "BIC-5000-24" #DEBUG
            
    #         if(ModelName_str == ""):
    #             ModelName_str = "-"
    #             print(f"ModelName = {ModelName_str}")
    #         else:
    #             GetScript()
    #             print("received modelname from C : " + ModelName_str)
                

    #         #Get CommInterface string
    #         CommInterface_uint8 = c_lib_Cali.Get_Communication_Type()
    #         CommInterface_str = comm_type_cases.get(CommInterface_uint8, "-") #"-" is default value
    #         print(f'CommInterface_str = {CommInterface_str}')

    #         #Get AdjustMode
    #         AdjustMode_uint8 = c_lib_Cali.Get_Keyboard_Adjustment()
    #         AdjustMode_str = adjust_mode_cases[AdjustMode_uint8]
    #         print(f'AdjustMode_str = {AdjustMode_str}')

    #         #Get CaliStatus
    #         CaliStatus_uint8 = c_lib_Cali.Get_Calibration_Status()
    #         CaliStatus_str = cali_status_cases[CaliStatus_uint8]
    #         print(f'CaliStatus_str = {CaliStatus_str}')

    #         print("\n")

    #         # If we already received the following things, the UI_stage can turn to stage 2
    #         # 1. CommInterface
    #         # 2. ModelName
    #         if(CommInterface_str != "-" and len(ModelName_str) > 6):                
    #             # UI_stage turns to next step
    #             UI_stage = 2
    #             print(UI_stage)
            
    #         if(CaliStatus_str == "FAIL"): #FAIL
    #             UI_stage = 0
    #             c_lib_Cali.Stop_Cali_thread()
    #             break
            
    #     else:

    #         #Get AdjustMode
    #         AdjustMode_uint8 = c_lib_Cali.Get_Keyboard_Adjustment()
    #         AdjustMode_str = adjust_mode_cases[AdjustMode_uint8]
    #         print(AdjustMode_str)

    #         #Get CaliStatus
    #         CaliStatus_uint8 = c_lib_Cali.Get_Calibration_Status()
    #         CaliStatus_str = cali_status_cases[CaliStatus_uint8]
    #         print(CaliStatus_str)
            
    #         #Add a task to the tasks_list 
    #         if((old_CaliType_str != "-" and old_CaliPoint_str != CaliPoint_str) or (old_CaliType_str != CaliType_str)):
    #             single_task['JSON_Content_Cali_Type'] = CaliType_str
    #             single_task['JSON_Content_Cali_Point'] = CaliPoint_str
    #             single_task['JSON_Content_Cali_Task_Status'] = "Running"
                
    #             tasks_list.append(single_task)
    #             single_task.clear()
            

    #     if(CaliStatus_uint8 == 1 or CaliStatus_uint8 == 2):
            
    #         #inform C_Cali_thread
    #         c_lib_Cali.Stop_Cali_thread()
    #         print("Thread : Cali terminated.\n")
    #         print("Thread : ServerTimer Terminated.\n")

    #         UI_stage = 0 # Waiting for button pressing of next time

    #         break #terminate ServerTimer
    #     time.sleep(0.1)

def GetScript():
    global CurrentFolder_str
    global ScriptFolderPath_str
    global ScriptFilePath_str
    global CurrentScript_dict
    global ScriptName_str
    global ModelName_str
    
    global Cali_point_step_cmd_mapping

    # ModelName_str = "BIC-5000-24" #DEBUG

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
        
def getModelName():
    ModelName_ptr = c_lib_Cali.Get_Machine_Name()
    ModelName_str = ModelName_ptr.decode("utf-8")
    
    #DEBUG
    # ModelName_str = "BIC-5000-24"
    return ModelName_str

def getCommInterface():
    CommInterface_uint8 = c_lib_Cali.Get_Communication_Type()
    CommInterface_str = comm_type_cases.get(CommInterface_uint8, "-") #"-" is default value
    
    #DEBUG
    # CommInterface_str = "CANBUS"
    return CommInterface_str

def getAdjustMode():
    AdjustMode_uint8 = c_lib_Cali.Get_Keyboard_Adjustment()
    AdjustMode_str = adjust_mode_cases[AdjustMode_uint8]

    #DEBUG
    # AdjustMode_str = adjust_mode_cases[0]
    return AdjustMode_str

def getCaliStatus():
    CaliStatus_uint8 = c_lib_Cali.Get_Calibration_Status()
    CaliStatus_str = cali_status_cases[CaliStatus_uint8]
    
    #DEBUG
    # CaliStatus_str = cali_status_cases[0]
    return CaliStatus_str

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

    c_lib_Cali.Start_Cali_thread()

    thread = Thread(target=server_timers)
    thread.daemon = True
    thread.start()
    return jsonify({})

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
    
    tmp_scaling_factor_str = CurrentScript_dict["CaliPoints_Info"][CaliPoint_str]["scaling_factor"]
    tmp_equipment_name_str = CurrentScript_dict["CaliPoints_Info"][CaliPoint_str]["Target_Equipment"]["Equipment_Name"]
    if(tmp_scaling_factor_str == "NO"):
        #For current cali point, there is no need of scaling_facotr
        tmp_done = 1
    else:
        #For current cali point, the scaling_factor is needed
        if(High_limit_str != "-" and Low_limit_str != "-"):
            tmp_done = 1
    
    

    data = jsonify({'WebAPI_CaliPoint'      : f'{CaliPoint_str}',
                    'WebAPI_Equipment_Name' : f'{tmp_equipment_name_str}',
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
    
    data = jsonify({'WebAPI_AdjustMode' : f'{AdjustMode_str}',
                    'WebAPI_CaliStatus' : f'{CaliStatus_str}',
                    'WebAPI_Target_EQ_value' : f'{TargetEquipment_Value_str}',
                    'WebAPI_change_point_flag' : change_to_next_cali_point_flag})
    
    if(change_to_next_cali_point_flag == 1):
        change_to_next_cali_point_flag = 0

    return data

@app.route('/api/scan_usb_devices', methods=['POST'])
def handle_scan_usb_devices():
    DeviceInfo_str_list = []

    #debug
    debug = 1

    #Real mode
    if(debug == 0):
        thread = Thread(target = c_lib_Search_Device.scan_usb_devices)
        thread.daemon = True
        thread.start()
        thread.join(timeout=10)
    
    
        DeviceInfo_str = ""
        DeviceName_str = ""
    
        count = 0
        #Get all devices' information from the Cali procedure
        while(1):
            DeviceInfo_ptr = c_lib_Search_Device.Get_Device_information(count)
            DeviceInfo_str = DeviceInfo_ptr.decode("utf-8")
                
            if(DeviceInfo_str == "Invalid Index" or DeviceInfo_str == ""):
                break
            else:
                DeviceInfo_str_list.append(DeviceInfo_str)

            count = count + 1
    
    #Debug mode
    elif(debug == 1):
        device_info_1 = "/dev/ttyUSB0,Chroma 51101-8"
        device_info_2 = "/dev/USBTMC0,Chroma ATE,66202,662025001432,1.70"
        DeviceInfo_str_list.append(device_info_1)
        DeviceInfo_str_list.append(device_info_2)
        print(DeviceInfo_str_list)
    
    
    #Load local deviceConfig json file
    try:
        with open(DEVICE_JSON_FILE_PATH, "r", encoding="utf-8") as file:
            devices_JSON = json.load(file)
    except FileNotFoundError:
        print("Error : JSON file not found")
    except json.JSONDecodeError:
        print("Error : Json format error")

    print(devices_JSON)

    #Parse the devices' information
    #Device_info_to_Web_dict = {
        # "Chroma ATE, 66202" : "DWAM"
    #}
    Device_info_to_Web_dict = {}
    
    EQ_type = ""

    for info in DeviceInfo_str_list:
        split_info_str_list = info.split(",")
        usb_port = split_info_str_list[0]

        if(split_info_str_list[1] == "Chroma 51101-8"):
            EQ_type = devices_JSON["Chroma 51101-8"]["EQ_TYPE"]
            Device_info_to_Web_dict["Chroma 51101-8"] = {}
            Device_info_to_Web_dict["Chroma 51101-8"]["EQ_TYPE"] = EQ_type
            Device_info_to_Web_dict["Chroma 51101-8"]["Serial_Num"] = ""
            Device_info_to_Web_dict["Chroma 51101-8"]["USB_Port"] = usb_port
        else:
            split_info_str_list = info.split(",")

            mfr_name = split_info_str_list[1]
            print(mfr_name)
            model_name = split_info_str_list[2]
            print(model_name)
            serial_num = split_info_str_list[3]
            print(serial_num)
            EQ_type = devices_JSON[mfr_name][model_name]["EQ_TYPE"]

            devices_JSON[mfr_name][model_name]["SerialNumber"].append(serial_num)

            Device_info_to_Web_dict[mfr_name + "," + model_name] = {}
            Device_info_to_Web_dict[mfr_name + "," + model_name]["EQ_TYPE"] = EQ_type
            Device_info_to_Web_dict[mfr_name + "," + model_name]["Serial_Num"] = serial_num
            Device_info_to_Web_dict[mfr_name + "," + model_name]["USB_Port"] = usb_port
    print(Device_info_to_Web_dict)
    return jsonify(Device_info_to_Web_dict), 200
    
@app.route('/api/save_devices_setting', methods=['POST'])
def handle_save_devices_setting():
    global Devices_setting
    try:
        Devices_setting = request.get_json()
        json_str = json.dumps(Devices_setting, indent=4)
        print(json_str)
        return jsonify({}), 200
    
    except Exception as e:
        return jsonify({'error' : str(e)}), 400

@app.route('/api/get_devices_setting', methods=['GET'])
def handle_get_devices_setting():
    print("HELLO")
    json_str = json.dumps(Devices_setting, indent=4)
    print(json_str)
    return jsonify(Devices_setting), 200

@app.route('/api/get_script_file_names', methods=['GET'])
def handle_get_script_file_names():
    files = os.listdir(ScriptFolderPath_str)
    print(files)
    return jsonify({"files" : files})

@app.route('/api/get_script_file_content', methods=['POST'])
def handle_get_script_file_content():
    try:
        Rcv_Json_data = request.get_json()
        script_File_Name = Rcv_Json_data.get('name', "File_Not_Found")
        print(script_File_Name)
        
        script_File_Path = os.path.join(ScriptFolderPath_str, script_File_Name)
        print(script_File_Path)

        with open(script_File_Path, 'r') as file:
            print("OPEN!!")
            return json.load(file), 200
    
    except Exception as e:
        return jsonify({'error' : str(e)}), 400
    
@app.route('/api/save_modified_script', methods=['POST'])
def handle_save_modified_script():
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
        # print(Rcv_Json_data)
        
        return jsonify({"message" : "JSON received", "data": Rcv_Json_data}), 200




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
    global enter_pressed
    Rcv_Json_data = request.get_json()
    enter_pressed = Rcv_Json_data["pressed"]
    print(f'enter_pressed = {enter_pressed}')
    return jsonify({}), 200

def Mock_equipment_value():
    return 24.86
##################################################
# main
##################################################
if __name__ == '__main__':
    app.run(debug=True)
    # GetScript()
    # ListScriptFiles()