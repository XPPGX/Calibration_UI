const socket = io.connect('http://localhost:5000')

document.addEventListener("keydown", function(event){
    if(event.code === "Space"){
    //prevent rolling page
    event.preventDefault(); 
    
    document.getElementById("UI_btnStartSetting").click();
    }
});


document.getElementById('UI_btnStartSetting').onclick = function(){
    socket.emit('ui_start_cali');
}

socket.on('update_1st_stage', function(data){
    console.log(data);
    document.getElementById('UI_ModelName').innerText = data.socket_ModelName;
    document.getElementById('UI_CommInterface').innerText = data.socket_CommInterface;
    document.getElementById('UI_AdjustMode').innerText = data.socket_AdjustMode
    document.getElementById('UI_Result').innerText = data.socket_CaliStatus;
});

socket.on('update_2nd_stage', function(data){
    console.log(data);
    document.getElementById('UI_CaliType').innerText      = data.socket_CaliType;
    document.getElementById('UI_Cali_point').innerText    = data.socket_CaliPoint;
    document.getElementById('UI_AdjustMode').innerText    = data.socket_AdjustMode;
    document.getElementById('UI_Result').innerText        = data.socket_CaliStatus;
})