document.getElementById("registerForm").addEventListener("submit", async (e) => {
    e.preventDefault();

    const email = document.getElementById("regEmail").value.trim();
    const nick = document.getElementById("regNick").value.trim();
    const passwd = document.getElementById("regPasswd").value.trim();
    const error = document.getElementById("regError");

    error.textContent = "";

    try {
        const returnedNick = await apiPost("/reg", { email, passwd, nick });

        localStorage.setItem("user_nick", returnedNick);

        window.location.href = "game_list.html";

        alert("Регистрация успешна! Добро пожаловать, " + returnedNick);
    } catch (err) {
        error.textContent = err.message;
    }
});

// Переключение вкладок
document.getElementById("registerTab").onclick = () => {
    document.getElementById("registerTab").classList.add("active");
    document.getElementById("loginTab").classList.remove("active");

    document.getElementById("registerForm").classList.remove("hidden");
    document.getElementById("loginForm").classList.add("hidden");
};
