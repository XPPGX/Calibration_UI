var switchPage_Enable_Flag = 0; //0 for enable, 1 for disable

function disable_switchPage()
{
    
    switchPage_Enable_Flag = 1;
    document.querySelectorAll('a').forEach(href_btn =>{
        href_btn.classList.add('disabled-link');
    });
    
}

function enable_switchPage()
{
    switchPage_Enable_Flag = 0;
    document.querySelectorAll('a').forEach(href_btn =>{
        href_btn.classList.remove('disabled-link');
    });
}

function initNavbar(){
    const hamburger = document.getElementById('hamburger');
    const menu = document.getElementById('menu');
    document.addEventListener('click', (event) => {
        if (!hamburger.contains(event.target) && !menu.contains(event.target)) {
            menu.classList.remove('open');
        }
    });

    hamburger.addEventListener('click', () => {
        menu.classList.toggle('open');
    });

    document.querySelectorAll('a').forEach(function(link){
        link.addEventListener('click', function(e){
            if(switchPage_Enable_Flag == 1)
            {
                e.preventDefault();
            }
            else
            {
                
            }
        });
    });
    
    function switchTab(tabIndex) {
        const content = document.getElementById('content');
        content.innerHTML = `<p>這是分頁 ${tabIndex} 的內容。</p>`;
        menu.classList.remove('open');
    }
}