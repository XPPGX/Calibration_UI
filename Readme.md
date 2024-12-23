# Used Threads 
1. UI_thread(main) => It controls the UI exploring or updating. 

2. flow_control_thread 	=> It controls the calibration process, where is polling and keyboard. The initial pollings(e.g. CANBUS & MODBUS) threads will be created here.

	> The two threads communicate with each other via global variables now. (The mutex should be considered).

# Compile
```bash
#type following command in terminal
$./compile.sh
```

Switch the content in "compile.sh", it will compile "UI_layout_main.c" or "Cali.c"
1. "Cali.c" 			=> executable file named 'b' => be compiled goes with flow_control_thread "UI_layout.c".

# Feature :
1. Combine UI and Cali.c
2. Get the information ("machine type", "communication interface", "cali type", "result")from machine and show then all on the UI
3. './kk' executable file : after press button, the Cali process runs. 
	> The pthread created in "UI_layout.c => on_activate()" impact which function is being compiled.

# To-do:
1. layout the right half components
2. Ask that "Cali_step" need what information



