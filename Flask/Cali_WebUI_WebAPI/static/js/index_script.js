// const socket = io.connect('http://localhost:5000');
var Cali_Flag = 0; //0 means calibration has not start yet, 1 means calibration already started
var UI_stage = 1;

document.addEventListener("keydown", function(event){
    if(event.key === ' '){ //key : SPACE
        //prevent rolling page
        event.preventDefault(); 
        
        document.getElementById("UI_btnStartSetting").click();
    }
});


document.getElementById('UI_btnStartSetting').addEventListener('click', async function(){
    console.log("Btn pressed\n");
    if(Cali_Flag == 0){
        // socket.emit('ui_start_cali');
        Cali_Flag = 1;

        const response = await fetch('/api/ui_start_cali', {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body:JSON.stringify({})
        });
        

    }
})

async function update_ui_stage(){
    if(Cali_Flag == 1){
        const stage_packet = await fetch('/api/stage_query')
        if(!stage_packet.ok){
            throw new Error("[Packet]was not ok");
        }
        const stage_JSON = await stage_packet.json();
        UI_stage = stage_JSON.WebAPI_Stage;
        console.log(`stage =  + ${UI_stage}`);

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
                break;
            case 2:
                console.log("2nd\n")
                const response_2 = await fetch('/api/update_ui_2nd_stage')
                if(!response_2.ok){
                    throw new Error("2nd network response was not ok");
                }
                const res2_data = await response_2.json();

                document.getElementById('UI_CaliType').innerText        = res2_data.WebAPI_CaliType;
                document.getElementById('UI_Cali_point').innerText      = res2_data.WebAPI_CaliPoint;
                document.getElementById('UI_AdjustMode').innerText      = res2_data.WebAPI_AdjustMode;
                document.getElementById('UI_Result').innerText          = res2_data.WebAPI_CaliStatus;
                break;
                
            default:
                break;
        }
    }
}

document.addEventListener('DOMContentLoaded', () => {
    setInterval(update_ui_stage, 100);
});

// socket.on('update_1st_stage', function(data){
//     console.log(data);
//     document.getElementById('UI_ModelName').innerText = data.socket_ModelName;
//     document.getElementById('UI_CommInterface').innerText = data.socket_CommInterface;
//     document.getElementById('UI_AdjustMode').innerText = data.socket_AdjustMode
//     document.getElementById('UI_Result').innerText = data.socket_CaliStatus;
// });

// socket.on('update_2nd_stage', function(data){
//     console.log(data);
//     document.getElementById('UI_CaliType').innerText      = data.socket_CaliType;
//     document.getElementById('UI_Cali_point').innerText    = data.socket_CaliPoint;
//     document.getElementById('UI_AdjustMode').innerText    = data.socket_AdjustMode;
//     document.getElementById('UI_Result').innerText        = data.socket_CaliStatus;
// })

// socket.on('update_terminate', function(data){
//     console.log(data)
//     setTimeout(() => {console.log("Wait 3 seconds");}, 3000);
//     Cali_Flag = 0;
// })