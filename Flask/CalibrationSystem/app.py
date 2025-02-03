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
        2 : "FAIL",
    }
)

CurrentFolder_str = os.path.dirname(__file__)
ScriptFolderPath_str = "./scripts"
ScriptFilePath_str = ""

UI_stage = 0
ModelName_str = "-"
CommInterface_str   = "-"
CaliType_str    = "-"
CaliPoint_str       = "-"
AdjustMode_str      = "-"
CaliStatus_str      = "-"

ScriptName_str = ""
CurrentScript_dict = {}
ScriptFolderPath_str = "./scripts"


app = Flask(__name__)
c_lib_Cali = ctypes.CDLL('./Clib/Cali.so')
c_lib_Search_Device = ctypes.CDLL('./Clib/Cali_Code/Auto_Search_Device.so')
##################################################
# C_lib parameters declaration
##################################################
c_lib_Cali.Get_Machine_Name.argtypes            = []
c_lib_Cali.Get_Machine_Name.restype             = ctypes.c_char_p

c_lib_Cali.Get_Communication_Type.argtypes      = []
c_lib_Cali.Get_Communication_Type.restype       = ctypes.c_uint8

c_lib_Cali.Get_Keyboard_Adjustment.argtypes     = []
c_lib_Cali.Get_Keyboard_Adjustment.restype      = ctypes.c_uint8

c_lib_Cali.Get_PSU_Calibration_Type.argtypes    = []
c_lib_Cali.Get_PSU_Calibration_Type.restype     = ctypes.c_uint8

c_lib_Cali.Get_Calibration_Status.argtypes      = []
c_lib_Cali.Get_Calibration_Status.restype       = ctypes.c_uint8

c_lib_Cali.Get_Calibration_Type_Point.argtypes  = []
c_lib_Cali.Get_Calibration_Type_Point.restype   = ctypes.c_uint8

c_lib_Cali.Start_Cali_thread.argtypes           = []
c_lib_Cali.Start_Cali_thread.restype            = None

c_lib_Cali.Stop_Cali_thread.argtypes            = []
c_lib_Cali.Stop_Cali_thread.restype             = None

c_lib_Search_Device.scan_usb_devices.argtypes   = []
c_lib_Search_Device.scan_usb_devices.restype    = None

c_lib_Search_Device.Get_Device_information      = []
c_lib_Search_Device.Get_Device_information      = ctypes.c_char_p

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

    single_task = {} #dictionary
    tasks_list = []

    old_CaliType_str = ""
    old_CaliPoint_str = ""

    #Initialize the strings
    if(UI_stage == 0):
        ModelName_str = "-"
        CommInterface_str = "-"
        CaliType_str = "-"
        CaliPoint_str = "-"
        AdjustMode_str = "微調"
        CaliStatus_str = "-"
        
        UI_stage = 1
    
    #This thread's main loop
    while True:
        
        if(UI_stage == 1):
            #Get ModelName string
            # ModelName_ptr = c_lib_Cali.Get_Machine_Name()
            # ModelName_str = ModelName_ptr.decode("utf-8")
            ModelName_str = "BIC-5000-24" #DEBUG
            
            if(ModelName_str == ""):
                ModelName_str = "-"
                print(f"ModelName = {ModelName_str}")
            else:
                GetScript()
                print("received modelname from C : " + ModelName_str)
                

            #Get CommInterface string
            CommInterface_uint8 = c_lib_Cali.Get_Communication_Type()
            CommInterface_str = comm_type_cases.get(CommInterface_uint8, "-") #"-" is default value
            print(f'CommInterface_str = {CommInterface_str}')

            #Get AdjustMode
            AdjustMode_uint8 = c_lib_Cali.Get_Keyboard_Adjustment()
            AdjustMode_str = adjust_mode_cases[AdjustMode_uint8]
            print(f'AdjustMode_str = {AdjustMode_str}')

            #Get CaliStatus
            CaliStatus_uint8 = c_lib_Cali.Get_Calibration_Status()
            CaliStatus_str = cali_status_cases[CaliStatus_uint8]
            print(f'CaliStatus_str = {CaliStatus_str}')

            print("\n")

            # If we already received the following things, the UI_stage can turn to stage 2
            # 1. CommInterface
            # 2. ModelName
            if(CommInterface_str != "-" and len(ModelName_str) > 6):                

                # UI_stage turns to next step
                UI_stage = 2
                print(UI_stage)
            
            if(CaliStatus_str == "FAIL"): #FAIL
                UI_stage = 0
                c_lib_Cali.Stop_Cali_thread()
                break
            
        else:
            #Get CaliType string
            CaliType_uint8 = c_lib_Cali.Get_PSU_Calibration_Type()
            CaliType_str = cali_type_cases.get(CaliType_uint8, "-")
            print(CaliType_str)

            #Get CaliPoint string
            CaliPoint_uint8 = c_lib_Cali.Get_Calibration_Type_Point()
            CaliPoint_str = str(CaliPoint_uint8)
            print(CaliPoint_str)

            #Get AdjustMode
            AdjustMode_uint8 = c_lib_Cali.Get_Keyboard_Adjustment()
            AdjustMode_str = adjust_mode_cases[AdjustMode_uint8]
            print(AdjustMode_str)

            #Get CaliStatus
            CaliStatus_uint8 = c_lib_Cali.Get_Calibration_Status()
            CaliStatus_str = cali_status_cases[CaliStatus_uint8]
            print(CaliStatus_str)
            
            #Add a task to the tasks_list 
            if((old_CaliType_str != "-" and old_CaliPoint_str != CaliPoint_str) or (old_CaliType_str != CaliType_str)):
                single_task['JSON_Content_Cali_Type'] = CaliType_str
                single_task['JSON_Content_Cali_Point'] = CaliPoint_str
                single_task['JSON_Content_Cali_Task_Status'] = "Running"
                
                tasks_list.append(single_task)
                single_task.clear()
            

        if(CaliStatus_uint8 == 1 or CaliStatus_uint8 == 2):
            
            #inform C_Cali_thread
            c_lib_Cali.Stop_Cali_thread()
            print("Thread : Cali terminated.\n")
            print("Thread : ServerTimer Terminated.\n")

            UI_stage = 0 # Waiting for button pressing of next time

            break #terminate ServerTimer
        time.sleep(0.1)

def GetScript():
    global CurrentFolder_str
    global ScriptFolderPath_str
    global ScriptFilePath_str
    global CurrentScript_dict
    global ScriptName_str
    global ModelName_str
    

    ModelName_str = "BIC-5000-24" #DEBUG

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
            print(CurrentScript_dict["ACV"]["enable"])

    except FileNotFoundError:
        print(f"[Error] File '{ScriptFilePath_str}' is not exist")
    except json.JSONDecodeError:
        print(f"[Error] File '{ScriptFilePath_str}' Invalid JSON format")
        



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
    # c_lib_Cali.Start_Cali_thread()
    
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
    global CaliType_str
    global CaliPoint_str
    global AdjustMode_str
    global CaliStatus_str
    
    data = jsonify({'WebAPI_CaliType'       : f'{CaliType_str}',
                    'WebAPI_CaliPoint'      : f'{CaliPoint_str}',
                    'WebAPI_AdjustMode'     : f'{AdjustMode_str}',
                    'WebAPI_CaliStatus'     : f'{CaliStatus_str}'})
    return data

@app.route('/api/scan_usb_devices', methods=['POST'])
def handle_scan_usb_devices():
    thread = Thread(target = c_lib_Search_Device.scan_usb_devices)
    thread.daemon = True
    thread.start()

    DeviceName_str = ""
    while(1):
        DeviceName_ptr = c_lib_Search_Device.Get_Device_information()
        DeviceName_str = DeviceName_ptr.decode("utf-8")
        if(DeviceName_str != ""):
            break
    thread.join(timeout=5)
    print(DeviceName_str)
    

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

        with open(script_File_Path + "test", "w", encoding="utf-8") as f:
            json.dump(Rcv_Json_data, f, indent=4, ensure_ascii=False)

        print(Rcv_Json_data)
        
        return jsonify({"message" : "JSON received", "data": Rcv_Json_data}), 200


##################################################
# main
##################################################
if __name__ == '__main__':
    app.run(debug=True)
    # GetScript()
    # ListScriptFiles()