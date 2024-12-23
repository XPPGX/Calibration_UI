# Compile
```bash
$./compile.sh
```

# Execute
```bash
$./main
```

# Description

## Used Threads 
1. UI_thread(main) => The entry of the whole process, it controls the UI exploring or updating. 

2. Cali_thread 	=> It controls the calibration process, where is polling and keyboard input. The initial pollings(e.g. CANBUS & MODBUS) threads will be created here.

> UI_thread communicate Cali_thread with the functions released by "Cali.C" and "Cali.h"

## Feature :
1. Combine UI and Cali.c
2. Get the information ("machine type", "communication interface", "cali type", "result", "cali step")from machine and show then all on the UI
3. './main' executable file : after press button "開始設定", the Cali process runs. 
	> The pthread created in "UI_layout.c => on_activate()" impact which function is being compiled.

## To-do:
1. layout the right half components



