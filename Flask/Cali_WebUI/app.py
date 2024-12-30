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
UI_Init_Flag = 0

ModelName_ptr = ctypes.c_char_p
ModelName_str = "-"

CommInterface_uint8 = ctypes.c_uint8
CommInterface_str   = "-"

CaliType_uint8  = ctypes.c_uint8
CaliType_str    = "-"

CaliPoint_uint8     = ctypes.c_uint8
CaliPoint_str       = "-"

AdjustMode_uint8    = ctypes.c_uint8
AdjustMode_str      = "-"

CaliStatus_uint8    = ctypes.c_uint8
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
socketio = SocketIO(app)
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

##################################################
# Functions implementation
##################################################
def server_timers():
    global UI_Init_Flag
    
    ModelName_ptr = c_lib_Cali.Get_Machine_Name()

    while True:
        # socketio.emit('update', {'socket_CaliStatus' : f'update at {time.time()}'})
        if(UI_Init_Flag == 0):
            #Get ModelName string
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
            socketio.emit('update_1st_stage', {
                'socket_ModelName' : f'{time.time()}, {ModelName_str}',
                'socket_CommInterface' : f'{time.time()}, {CommInterface_str}',
                'socket_AdjustMode' : f'{time.time()}, {AdjustMode_str}',
                'socket_CaliStatus' : f'{time.time()}, {CaliStatus_str}'
            })

            # UI_Init_Flag = 1
            if(CommInterface_str != "-"):
                UI_Init_Flag = 1
                print(UI_Init_Flag)
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

            socketio.emit('update_2nd_stage', {
                'socket_CaliType' : f'{time.time()}, {CaliType_str}',
                'socket_CaliPoint' : f'{time.time()}, {CaliPoint_str}',
                'socket_AdjustMode' : f'{time.time()}, {AdjustMode_str}',
                'socket_CaliStatus' : f'{time.time()}, {CaliStatus_str}'
            })

        time.sleep(0.1)

##################################################
# Web page routes
##################################################
@app.route('/')
def index():
    return render_template('index.html')


##################################################
# Socket services
##################################################
@socketio.on('connect')
def handle_connect():
    print('Client connected')

@socketio.on('ui_start_cali')
def handle_ui_start_cali():
#     UI_Init_Flag = 0
    
#     ModelName_ptr = c_lib_Cali.Get_Machine_Name()
#     ModelName_str = "-"

#     CommInterface_uint8 = 0
#     CommInterface_str   = "-"

#     CaliType_uint8      = 0
#     CaliType_str        = "-"

#     CaliPoint_uint8     = 0
#     CaliPoint_str       = "-"

#     AdjustMode_uint8    = 0
#     AdjustMode_str      = "微調"

#     CaliStatus_uint8    = 0
#     CaliStatus_str      = "Running..."

    thread = Thread(target=server_timers)
    thread.daemon = True
    thread.start()
    
    c_lib_Cali.Start_Cali_thread()

##################################################
# main
##################################################
if __name__ == '__main__':
    socketio.run(app, debug=True)
