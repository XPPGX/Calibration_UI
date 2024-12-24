from flask import Flask, render_template, request, jsonify
import ctypes

app = Flask(__name__)

c_library = ctypes.CDLL('./Clib/func.so')

c_library.my_func.restype = ctypes.c_int
c_library.my_func.argtypes = [ctypes.c_int, ctypes.c_double]

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/api/call', methods=['POST'])
def call_c_function():
    data = request.json
    arg1 = 10
    arg2 = 3.14
    result = c_library.my_func(arg1, arg2)

    return jsonify({'result' : result})

if __name__ == '__main__':
    app.run(debug=True)