document.getElementById("loginForm").addEventListener("submit", async (e) => {
    e.preventDefault();

    const login = document.getElementById("loginLogin").value.trim();
    const passwd = document.getElementById("loginPasswd").value.trim();
    const error = document.getElementById("loginError");

    localStorage.setItem("user_nick", login);

    error.textContent = "";

    try {
        const token = await apiPost("/login", { login, passwd });

        // сохраняем токен
        localStorage.setItem("auth_token", token);

        // автоматический переход
        window.location.href = "game_list.html";

    } catch (err) {
        error.textContent = err.message;
    }
});

// Переключение вкладок
document.getElementById("loginTab").onclick = () => {
    document.getElementById("loginTab").classList.add("active");
    document.getElementById("registerTab").classList.remove("active");

    document.getElementById("loginForm").classList.remove("hidden");
    document.getElementById("registerForm").classList.add("hidden");
};
