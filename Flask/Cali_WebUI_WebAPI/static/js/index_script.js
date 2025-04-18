// const socket = io.connect('http://localhost:5000');
/**
 * Cali_Flag :
 * 0 means calibration has not start yet
 * 1 means calibration already started
 * 2 means calibration finish and the values still remain on the screen
 */
var Cali_Flag = 0; 

/**
 * UI_stage :
 * 1 means UI stage 1
 * 2 means UI stage 2
 */
var UI_stage = 1;


var UI_AdjustMode_new_style = 'big_wood_style';

var UI_Result_color = '';
var UI_Result_color_map = {
    "FINISH" : 'green_style',
    "FAIL" : "red_style"
};

const taskList = document.getElementById('taskList');


document.addEventListener("keydown", function(event){
    if(event.key === ' '){ //key == SPACE
        //prevent rolling page
        event.preventDefault(); 
        
        document.getElementById("UI_btnStartSetting").click();
    }
});


document.getElementById('UI_btnStartSetting').addEventListener('click', async function(){
    console.log("Btn pressed\n");
    let old_UI_CaliType_str;
    let old_UI_Cali_point_str;
    switch(Cali_Flag){
        case 2:
            document.getElementById('UI_ModelName').innerText = "-";
            document.getElementById('UI_CommInterface').innerText = "-";
            document.getElementById('UI_CaliType').innerText = "-";
            document.getElementById('UI_Cali_point').innerText = "-";

            document.getElementById('UI_AdjustMode').innerText = "微調";
            if(document.getElementById('UI_AdjustMode_div').classList.contains(UI_AdjustMode_new_style)){
                document.getElementById('UI_AdjustMode_div').classList.remove(UI_AdjustMode_new_style);
            }

            document.getElementById('UI_Result').innerText = "-";
            if(document.getElementById('UI_Result').classList.contains(UI_Result_color)){
                document.getElementById('UI_Result').classList.remove(UI_Result_color);
            }

        case 0:
            Cali_Flag = 1;
            UI_stage = 1;
            
            const response = await fetch('/api/ui_start_cali', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body:JSON.stringify({})
            });
            break;
        default:
            break;
    }
})

async function update_ui_stage(){
    if(Cali_Flag == 1){

        switch(UI_stage){
            case 1:
                console.log("1st\n")
                const response_1 = await fetch('/api/update_ui_1st_stage')
                if(!response_1.ok){
                    throw new Error("1st network response was not ok");
                }
                const res1_data = await response_1.json();
                    
                document.getElementById('UI_ModelName').innerText       = res1_data.WebAPI_ModelName;
                document.getElementById('UI_CommInterface').innerText   = res1_data.WebAPI_CommInterface;
                document.getElementById('UI_AdjustMode').innerText      = res1_data.WebAPI_AdjustMode;
                document.getElementById('UI_Result').innerText          = res1_data.WebAPI_CaliStatus;

                if(document.getElementById('UI_CommInterface').innerText != "-"){
                    setTimeout(() => {
                        UI_stage = 2;
                    }, 0);
                }
                
                if(document.getElementById('UI_Result').innerText == "FAIL"){
                    UI_Result_color = UI_Result_color_map[document.getElementById('UI_Result').innerText];
                    document.getElementById('UI_Result').classList.add(UI_Result_color);
                    setTimeout(() => {
                        UI_stage = 3;
                        Cali_Flag = 2;
                    });
                }

                break;
                
            case 2:
                console.log("2nd\n")
                const response_2 = await fetch('/api/update_ui_2nd_stage')
                if(!response_2.ok){
                    throw new Error("2nd network response was not ok");
                }
                const res2_data = await response_2.json();

                
                old_UI_CaliType_str = document.getElementById('UI_CaliType').innerText;
                old_UI_Cali_point_str = document.getElementById('UI_Cali_point').innerText;

                document.getElementById('UI_CaliType').innerText        = res2_data.WebAPI_CaliType;
                document.getElementById('UI_Cali_point').innerText      = res2_data.WebAPI_CaliPoint;
                document.getElementById('UI_AdjustMode').innerText      = res2_data.WebAPI_AdjustMode;
                document.getElementById('UI_Result').innerText          = res2_data.WebAPI_CaliStatus;

                let new_UI_CaliType_str = res2_data.WebAPI_CaliType;
                let new_UI_Cali_Point_str = res2_data.WebAPI_CaliPoint;

                
                
                //[Bug] If the cali point encountered with non-ordered sequence, the following process may show the wrong cali point on the web site
                //Add a task that is consisted of calibration type and point.
                if((old_UI_CaliType_str != "-" && old_UI_Cali_point_str != new_UI_Cali_Point_str) ||
                 (old_UI_CaliType_str != new_UI_CaliType_str)){
                    addTask(`${document.getElementById('UI_CaliType').innerText},  ${document.getElementById('UI_Cali_point').innerText}`);
                    
                    //modify the status of last taskItem to be OK
                    const tasks = document.querySelectorAll('.task-item');
                    if(tasks.length > 1){
                        const lastTask = tasks[tasks.length - 2];
                        const lastTaskStatus = lastTask.querySelector('.task-status');
                        lastTaskStatus.textContent = 'OK';
                        lastTaskStatus.className = 'task-status status-ok';
                    }
                }
                


                //Change the color of adjustment after pressing space key
                if(document.getElementById('UI_AdjustMode').innerText == "粗調"){
                    document.getElementById('UI_AdjustMode_div').classList.add(UI_AdjustMode_new_style);
                }
                else if(document.getElementById('UI_AdjustMode').innerText == "微調"){
                    if(document.getElementById('UI_AdjustMode_div').classList.contains(UI_AdjustMode_new_style)){
                        document.getElementById('UI_AdjustMode_div').classList.remove(UI_AdjustMode_new_style);
                    }
                }

                
                if(document.getElementById('UI_Result').innerText == "FINISH" 
                || document.getElementById('UI_Result').innerText == "FAIL"){
                    UI_Result_color = UI_Result_color_map[document.getElementById('UI_Result').innerText];
                    document.getElementById('UI_Result').classList.add(UI_Result_color);
                    setTimeout(() => {
                        UI_stage = 3;
                        Cali_Flag = 2;
                    });

                    // const tasks = document.querySelectorAll('.task-item');
                    // if(tasks.length > 1){
                    //     const lastTask = tasks[tasks.length - 1];
                    //     const lastTaskStatus = lastTask.querySelector('.task-status');
                    //     lastTaskStatus.textContent = 'FAIL';
                    //     lastTaskStatus.className = 'task-status status-fail';
                    // }
                }
                
                break;
                
            default:
                break;
        }
    }
}

function addTask(taskName){
    const taskItem = document.createElement('div');
    taskItem.className = 'task-item';

    const taskNameSpan = document.createElement('span');
    taskNameSpan.className = 'task-name';
    taskNameSpan.textContent = taskName;

    const taskStatusSpan = document.createElement('span');
    taskStatusSpan.className = `task-status status-running`;
    taskStatusSpan.textContent = 'Running';


    taskItem.appendChild(taskNameSpan);
    taskItem.appendChild(taskStatusSpan);

    taskList.appendChild(taskItem);
}


document.getElementById('showTasksBtn').addEventListener('click', async function(){
   taskList.classList.remove('hidden');
   document.getElementById('table').classList.add('hidden');
   document.getElementById('showTasksBtn').classList.add('btn-active');
   document.getElementById('showValuesBtn').classList.remove('btn-active');
})


document.getElementById('showValuesBtn').addEventListener('click', async function(){
   taskList.classList.add('hidden');
   document.getElementById('table').classList.remove('hidden');
   document.getElementById('showValuesBtn').classList.add('btn-active');
   document.getElementById('showTasksBtn').classList.remove('btn-active');
})

document.addEventListener('DOMContentLoaded', () => {
    setInterval(update_ui_stage, 100);
});
