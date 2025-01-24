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

    function switchTab(tabIndex) {
        const content = document.getElementById('content');
        content.innerHTML = `<p>這是分頁 ${tabIndex} 的內容。</p>`;
        menu.classList.remove('open');
    }
}