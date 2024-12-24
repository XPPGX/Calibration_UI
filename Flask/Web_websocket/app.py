from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit
import time
import ctypes

app = Flask(__name__)
socketio = SocketIO(app)

c_library = ctypes.CDLL('./Clib/func.so')

c_library.my_func.restype = ctypes.c_int
c_library.my_func.argtypes = [ctypes.c_int, ctypes.c_double]


@app.route('/')
def index():
    return render_template('index.html')

@socketio.on('connect')
def handle_connect():
    print('Client connected')

def send_updates():
    while True:
        led_status = c_library.get_led_status()
        socketio.emit('update', {'message' : f'Updated at {time.time()}, led_status = {led_status}'})
        time.sleep(0.1)

from threading import Thread
thread = Thread(target=send_updates)
thread.daemon = True
thread.start()


@app.route('/api/call', methods=['POST'])
def call_c_function():
    data = request.json
    arg1 = 10
    arg2 = 3.14
    result = c_library.my_func(arg1, arg2)
    
    c_library.entry()
    return jsonify({'result' : result})

if __name__ == '__main__':
    socketio.run(app, debug=True)