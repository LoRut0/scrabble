async function loadGameList() {
    const token = localStorage.getItem("auth_token");
    const gameListDiv = document.getElementById("gameList");
    const error = document.getElementById("gameError");

    if (!token) {
        window.location.href = "index.html";
        return;
    }

    error.textContent = "";
    gameListDiv.innerHTML = "Загрузка...";

    try {
        const raw = await apiPost("/game", {
            action: "list",
            token: token
        });

        const data = JSON.parse(raw);

        gameListDiv.innerHTML = "";

        // учитывай реальное имя поля: game_info_list или game_unfo_list
        const list = data.game_info_list || data.game_unfo_list || [];

        if (list.length === 0) {
            gameListDiv.textContent = "Пока нет доступных игр.";
            return;
        }

        list.forEach(game => {
            const item = document.createElement("div");
            item.className = "game-item";

            const info = document.createElement("div");
            info.className = "game-info";
            info.innerHTML = `
                <b>Игра #${game.game_id}</b>
                Хост: ${game.host_user_name}<br>
                Игроков: ${game.num_of_users}
            `;

            const actions = document.createElement("div");
            actions.className = "game-actions";

            const joinBtn = document.createElement("button");
            joinBtn.className = "join-btn";
            joinBtn.textContent = "Войти";
            joinBtn.onclick = () => joinGame(game.game_id);

            const endBtn = document.createElement("button");
            endBtn.className = "end-btn";
            endBtn.textContent = "Завершить";
            endBtn.onclick = () => endGame(game.game_id);

            actions.appendChild(joinBtn);
            actions.appendChild(endBtn);

            item.appendChild(info);
            item.appendChild(actions);
            gameListDiv.appendChild(item);
        });

    } catch (err) {
        error.textContent = "Ошибка загрузки игр: " + err.message;
        gameListDiv.innerHTML = "";
    }
}

async function createGame() {
    const token = localStorage.getItem("auth_token");
    const error = document.getElementById("gameError");

    if (!token) {
        window.location.href = "index.html";
        return;
    }

    error.textContent = "";

    try {
        const id = await apiPost("/game", {
            action: "create",
            token: token
        });

        alert("Игра создана! ID: " + id);
        loadGameList();

    } catch (err) {
        error.textContent = "Ошибка создания игры: " + err.message;
    }
}

async function joinGame(gameId) {
    const token = localStorage.getItem("auth_token");

    if (!token) {
        window.location.href = "index.html";
        return;
    }

    try {
        const id = await apiPost("/game", {
            action: "join",
            token: token,
            game_id: gameId
        });

        alert("Вы подключились к игре #" + id);
        // сюда потом добавим переход в комнату
        // window.location.href = "room.html?game_id=" + id;

        window.location.href = "game_room.html?game_id=" + id;

    } catch (err) {
        alert("Ошибка подключения: " + err.message);
    }

}

async function endGame(gameId) {
    const token = localStorage.getItem("auth_token");

    if (!token) {
        window.location.href = "index.html";
        return;
    }

    if (!confirm(`Завершить игру #${gameId}?`)) return;

    try {
        const id = await apiPost("/game", {
            action: "end",
            token: token,
            game_id: gameId
        });

        alert("Игра #" + id + " завершена.");
        loadGameList();
    } catch (err) {
        alert("Ошибка завершения игры: " + err.message);
    }
}

document.getElementById("createGameBtn").onclick = createGame;
window.onload = loadGameList;
