let ws = null;
let gameId = null;
let token = localStorage.getItem("auth_token");

let pendingPlacements = []; // [{x, y, letter}]
let lastScorePreview = null;

// --- utils ---
function qs(id) {
    return document.getElementById(id);
}

function sendWS(data) {
    console.log("WS SEND:", data);
    data.token = token;
    ws.send(JSON.stringify(data));
}

// --- init ---
function initGameRoom() {
    const params = new URLSearchParams(window.location.search);
    gameId = Number(params.get("game_id"));

    if (!gameId) {
        alert("game_id not provided");
        return;
    }

    ws = new WebSocket(`ws://localhost:8080/ws`);

    ws.onopen = () => {
        sendWS({ action: "start" });
        startStatePolling();
    };

    ws.onmessage = (event) => {
        const msg = JSON.parse(event.data);
        handleMessage(msg);
    };

    ws.onerror = () => {
        alert("WebSocket error");
    };

    initButtons();
}

// --- polling ---
function startStatePolling() {
    setInterval(() => {
        sendWS({
            action: "state",
            game_id: gameId
        });
        console.log("polled state");
    }, 1000);
    console.log("started polling");
}

// --- handlers ---
function handleMessage(msg) {
    if (msg.letters && msg.prices) {
        renderBoard(msg.letters, msg.prices);
    }

    if (msg.tiles) {
        renderTiles(msg.tiles);
    }

    if (typeof msg.score === "number") {
        qs("tilesLeft").innerText = msg.tiles_left ?? 0;
    }

    if (msg.action === "place") {
        lastScorePreview = msg.score;
        if (msg.score < 0) {
            alert("Некорректное размещение");
        }
    }

    if (msg.action === "submit") {
        if (msg.score >= 0) {
            pendingPlacements = [];
        } else {
            alert("Нечего подтверждать");
        }
    }
}

// --- board ---
function renderBoard(letters, prices) {
    const board = qs("gameBoard");
    board.innerHTML = "";

    const size = letters.length;
    board.style.display = "grid";
    board.style.gridTemplateColumns = `repeat(${size}, 1fr)`;
    board.style.gridTemplateRows = `repeat(${size}, 1fr)`;

    for (let y = 0; y < size; y++) {
        for (let x = 0; x < size; x++) {
            const cell = document.createElement("div");
            cell.className = "board-cell";
            cell.dataset.x = x;
            cell.dataset.y = y;

            cell.style.border = "1px solid black";
            cell.style.display = "flex";
            cell.style.alignItems = "center";
            cell.style.justifyContent = "center";
            cell.style.fontSize = "20px";

            if (letters[y][x] !== " ") {
                cell.textContent = letters[y][x];
            }

            cell.ondragover = (e) => e.preventDefault();
            cell.ondrop = (e) => onDropLetter(e, x, y);

            board.appendChild(cell);
        }
    }
}

// --- tiles ---
function renderTiles(tiles) {
    const panel = qs("lettersPanel");
    panel.innerHTML = "";

    tiles.forEach((letter, idx) => {
        const tile = document.createElement("div");
        tile.className = "letter-tile";
        tile.textContent = letter;
        tile.draggable = true;

        tile.ondragstart = (e) => {
            e.dataTransfer.setData("letter", letter);
            e.dataTransfer.setData("index", idx);
        };

        panel.appendChild(tile);
    });
}

// --- drag/drop ---
function onDropLetter(event, x, y) {
    const letter = event.dataTransfer.getData("letter");
    if (!letter) return;

    pendingPlacements.push({ x, y, letter });

    sendWS({
        action: "place",
        game_id: gameId,
        coordinates: pendingPlacements.map(p => [p.x, p.y]),
        letters: pendingPlacements.map(p => p.letter).join("")
    });
}

// --- buttons ---
function initButtons() {
    qs("btnSend").onclick = () => {
        sendWS({
            action: "submit",
            game_id: gameId
        });
    };

    qs("btnPass").onclick = () => {
        sendWS({
            action: "pass",
            game_id: gameId
        });
    };

    qs("btnSwap").onclick = () => {
        sendWS({
            action: "change",
            game_id: gameId
        });
    };
}

window.onload = initGameRoom;
