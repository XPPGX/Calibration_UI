from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
from datetime import datetime
import time
import ctypes
from threading import Thread
from collections import defaultdict

##################################################
# Global Variables
##################################################
UI_stage = 0


ModelName_str = "-"
CommInterface_str   = "-"
CaliType_str    = "-"
CaliPoint_str       = "-"
AdjustMode_str      = "-"
CaliStatus_str      = "-"

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

app = Flask(__name__)
c_lib_Cali = ctypes.CDLL('./Clib/Cali.so')

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
    if(UI_stage == 0):
        ModelName_str = "-"
        CommInterface_str = "-"
        CaliType_str = "-"
        CaliPoint_str = "-"
        AdjustMode_str = "微調"
        CaliStatus_str = "-"
        
        UI_stage = 1
    while True:
        # socketio.emit('update', {'socket_CaliStatus' : f'update at {time.time()}'})
        if(UI_stage == 1):
            #Get ModelName string
            ModelName_ptr = c_lib_Cali.Get_Machine_Name()
            ModelName_str = ModelName_ptr.decode("utf-8")
            if(ModelName_str == ""):
                ModelName_str = "-"
                print(f"ModelName = {ModelName_str}")
            else:
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
            
            # socketio.emit('update_1st_stage', {
            #     'socket_ModelName' : f'{time.time()}, {ModelName_str}',
            #     'socket_CommInterface' : f'{time.time()}, {CommInterface_str}',
            #     'socket_AdjustMode' : f'{time.time()}, {AdjustMode_str}',
            #     'socket_CaliStatus' : f'{time.time()}, {CaliStatus_str}'
            # })

            # UI_stage = 1
            if(CommInterface_str != "-"):
                UI_stage = 2
                print(UI_stage)
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

            # socketio.emit('update_2nd_stage', {
            #     'socket_CaliType' : f'{time.time()}, {CaliType_str}',
            #     'socket_CaliPoint' : f'{time.time()}, {CaliPoint_str}',
            #     'socket_AdjustMode' : f'{time.time()}, {AdjustMode_str}',
            #     'socket_CaliStatus' : f'{time.time()}, {CaliStatus_str}'
            # })

        if(CaliStatus_uint8 == 1 or CaliStatus_uint8 == 2):
            
            #inform C_Cali_thread
            c_lib_Cali.Stop_Cali_thread()
            print("Thread : Cali terminated.\n")
            print("Thread : ServerTimer Terminated.\n")

            #inform javascript

            # socketio.emit('update_terminate', {})

            print("Wait 3 seconds\n") # For waiting can0 bus down and C_Cali_thread is truly terminated
            time.sleep(3)

            #update flask variable
            UI_stage = 0
            

            
            break #terminate ServerTimer
        time.sleep(0.1)

##################################################
# Web page routes
##################################################
@app.route('/')
def index():
    return render_template('index.html')


##################################################
# WebAPI
##################################################
@app.route('/api/ui_start_cali', methods=['POST'])
def handle_ui_start_cali():
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

    # data = jsonify({'WebAPI_ModelName'      : f'Time = {time.time()}, {ModelName_str}',
    #                 'WebAPI_CommInterface'  : f'Time = {time.time()}, {CommInterface_str}',
    #                 'WebAPI_AdjustMode'     : f'Time = {time.time()}, {AdjustMode_str}',
    #                 'WebAPI_CaliStatus'     : f'Time = {time.time()}, {CaliStatus_str}'})
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
    
    # data = jsonify({'WebAPI_CaliType'       : f'Time = {time.time()}, {CaliType_str}',
    #                 'WebAPI_CaliPoint'      : f'Time = {time.time()}, {CaliPoint_str}',
    #                 'WebAPI_AdjustMode'     : f'Time = {time.time()}, {AdjustMode_str}',
    #                 'WebAPI_CaliStatus'     : f'Time = {time.time()}, {CaliStatus_str}'})
    data = jsonify({'WebAPI_CaliType'       : f'{CaliType_str}',
                    'WebAPI_CaliPoint'      : f'{CaliPoint_str}',
                    'WebAPI_AdjustMode'     : f'{AdjustMode_str}',
                    'WebAPI_CaliStatus'     : f'{CaliStatus_str}'})
    return data


##################################################
# main
##################################################
if __name__ == '__main__':
    app.run(debug=True)
