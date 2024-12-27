from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
import time
import ctypes
from threading import Thread
from collections import defaultdict

########### Global Variables ############
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
#########################################

app = Flask(__name__)
socketio = SocketIO(app)
c_lib_Cali = ctypes.CDLL('./Clib/Cali.so')

c_lib_Cali.Get_Machine_Name.restype             = ctypes.c_char_p
c_lib_Cali.Get_Communication_Type.restype       = ctypes.c_uint8
c_lib_Cali.Get_Keyboard_Adjustment.restype      = ctypes.c_uint8
c_lib_Cali.Get_PSU_Calibration_Type.restype     = ctypes.c_uint8
c_lib_Cali.Get_Calibration_Status.restype       = ctypes.c_uint8
c_lib_Cali.Get_Calibration_Type_Point.restype   = ctypes.c_uint8

ModelName_ptr = c_lib_Cali.Get_Machine_Name()

def server_timers():
    global UI_Init_Flag

    while True:
        if(UI_Init_Flag == 0):
            #Get ModelName string
            ModelName_str = ModelName_ptr.decode("utf-8")

            #Get CommInterface string
            CommInterface_uint8 = c_lib_Cali.Get_Communication_Type()
            CommInterface_str = comm_type_cases.get(CommInterface_uint8, "-") #"-" is default value

            #Get AdjustMode
            AdjustMode_uint8 = c_lib_Cali.Get_Keyboard_Adjustment()
            AdjustMode_str = adjust_mode_cases[AdjustMode_uint8]

            #Get CaliStatus
            CaliStatus_uint8 = c_lib_Cali.Get_Calibration_Status()
            CaliStatus_str = cali_status_cases[CaliStatus_uint8]

            socketio.emit('update', {
                'socket_ModelName' : f'{ModelName_str}',
                'socket_CommInterface' : f'{CommInterface_str}',
                'socket_AdjustMode' : f'{AdjustMode_str}',
                'socket_CaliStatus' : f'{CaliStatus_str}'
            })

            if(CommInterface_str != "-"):
                UI_Init_Flag = 1
        else:
            #Get CaliType string
            CaliType_uint8 = c_lib_Cali.Get_PSU_Calibration_Type()
            CaliType_str = cali_type_cases.get(CaliType_uint8, "-")

            #Get CaliPoint string
            CaliPoint_uint8 = c_lib_Cali.Get_Calibration_Type_Point()
            CaliPoint_str = str(CaliPoint_uint8)

            #Get AdjustMode
            AdjustMode_uint8 = c_lib_Cali.Get_Keyboard_Adjustment()
            AdjustMode_str = adjust_mode_cases[AdjustMode_uint8]

            #Get CaliStatus
            CaliStatus_uint8 = c_lib_Cali.Get_Calibration_Status()
            CaliStatus_str = cali_status_cases[CaliStatus_uint8]

            socketio.emit('update', {
                'socket_CaliType' : f'{CaliType_str}',
                'socket_CaliPoint' : f'{CaliPoint_str}',
                'socket_AdjustMode' : f'{AdjustMode_str}',
                'socket_CaliStatus' : f'{CaliStatus_str}'
            })

        time.sleep(0.1)


@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('connect')
def handle_connect():
    print('Client connected')

@socketio.on('ui_start_cali')
def handle_ui_start_cali():
    UI_Init_Flag = 0
    
    ModelName_ptr = c_lib_Cali.Get_Machine_Name()
    ModelName_str = "-"

    CommInterface_uint8 = 0
    CommInterface_str   = "-"

    CaliType_uint8      = 0
    CaliType_str        = "-"

    CaliPoint_uint8     = 0
    CaliPoint_str       = "-"

    AdjustMode_uint8    = 0
    AdjustMode_str      = "微調"

    CaliStatus_uint8    = 0
    CaliStatus_str      = "Running..."

    thread = Thread(target=server_timers)
    thread.daemon = True
    thread.start()

    socketio.emit('WebHandler_init_ui', {
        'socket_ModelName' : f'{ModelName_str}',
        'socket_CommInterface' : f'{CommInterface_str}',
        'socket_CaliType' : f'{CaliType_str}',
        'socket_CaliPoint' : f'{CaliPoint_str}',
        'socket_AdjustMode' : f'{AdjustMode_str}',
        'socket_CaliStatus' : f'{CaliStatus_str}'
    })

if __name__ == '__main__':
    socketio.run(app, debug=True)
