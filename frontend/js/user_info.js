function updateUserInfo() {
    const nick = localStorage.getItem("user_nick");
    const link = document.getElementById("userLink");

    if (!nick) {
        link.textContent = "Не вошли";
        link.href = "index.html";
        return;
    }

    link.textContent = nick;
}
