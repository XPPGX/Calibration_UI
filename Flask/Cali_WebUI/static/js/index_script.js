const socket = io.connect('http://localhost:5000');
var Cali_Flag = 0; //0 means calibration has not start yet, 1 means calibration already started

document.addEventListener("keydown", function(event){
    if(event.key === ' '){ //key : SPACE
        //prevent rolling page
        event.preventDefault(); 
        
        document.getElementById("UI_btnStartSetting").click();
    }
});


document.getElementById('UI_btnStartSetting').onclick = function(){
    console.log("Btn pressed\n");
    if(Cali_Flag == 0){
        socket.emit('ui_start_cali');
        Cali_Flag = 1;
    }
}

socket.on('update_init', function(){
    document.getElementById('UI_ModelName').innerText = "-";
    document.getElementById('UI_CommInterface').innerText = "-";
    document.getElementById('UI_CaliType').innerText      = "-";
    document.getElementById('UI_Cali_point').innerText    = "-";
    document.getElementById('UI_AdjustMode').innerText = "微調";
    document.getElementById('UI_Result').innerText = "-";
});

socket.on('update_1st_stage', function(data){
    console.log(data);
    document.getElementById('UI_ModelName').innerText = data.socket_ModelName;
    document.getElementById('UI_CommInterface').innerText = data.socket_CommInterface;
    document.getElementById('UI_AdjustMode').innerText = data.socket_AdjustMode
    document.getElementById('UI_Result').innerText = data.socket_CaliStatus;

    if(document.getElementById('UI_Result').innerHTML == "FINISH"){
        document.getElementById('UI_Result').classList.add('green_style');
    }
    else if(document.getElementById('UI_Result').innerHTML == "FAIL"){
        document.getElementById('UI_Result').classList.add('red_style');
    }
});

socket.on('update_2nd_stage', function(data){
    console.log(data);
    document.getElementById('UI_CaliType').innerText      = data.socket_CaliType;
    document.getElementById('UI_Cali_point').innerText    = data.socket_CaliPoint;
    document.getElementById('UI_AdjustMode').innerText    = data.socket_AdjustMode;
    document.getElementById('UI_Result').innerText        = data.socket_CaliStatus;
    if(document.getElementById('UI_Result').innerHTML == "FINISH"){
        document.getElementById('UI_Result').classList.add('green_style');
    }
    else if(document.getElementById('UI_Result').innerHTML == "FAIL"){
        document.getElementById('UI_Result').classList.add('red_style');
    }
})

socket.on('update_terminate', function(data){
    console.log(data)
    setTimeout(() => {console.log("Wait 3 seconds");}, 3000);
    Cali_Flag = 0;
})
