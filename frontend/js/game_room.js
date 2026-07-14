/*
 * Scrabble game-room client.
 *
 * WebSocket protocol (as implemented by the backend):
 *   - First message after connect MUST be the auth frame: {token, game_id}.
 *   - Then action frames: {action: "state" | "place" | "submit" | "pass" | "change", ...}.
 *   - Server replies are either  {error: "..."}  or a full state snapshot:
 *       {
 *         public: {
 *           current_player: <index into players>,
 *           bag_size: <int>,
 *           players: [{id, score}, ...],
 *           letters: letters[x][y]  (" " == empty),
 *           prices:  prices[x][y]   (-1 == plain cell)
 *         },
 *         private: {
 *           hand: [<char>, ...],
 *           pending_score: <int>    // present ONLY when it is THIS player's turn
 *         }
 *       }
 *   - A successful place/submit/pass/change broadcasts a fresh state to every
 *     connected player, so opponent moves arrive as pushes (we also poll lightly).
 *   - `place` does NOT put tiles on the board; it only (re)computes pending_score
 *     for the full set of pending tiles sent each time. Tiles land on the board
 *     only on `submit`. So we keep a local pending overlay until then.
 */

const WS_URL = API_BASE.replace(/^http/, "ws") + "/ws";

const token = localStorage.getItem("auth_token");

let ws = null;
let gameId = null;

let lastState = null;        // last full state snapshot from the server
let started = false;         // game is ongoing (past the lobby)
let myTurn = false;          // derived from presence of private.pending_score

let pending = [];            // tiles placed this turn but not submitted: {x, y, letter, handIndex}
let selectedHandIndex = null;// hand tile chosen for the next board placement
let swapMode = false;        // selecting tiles to exchange
let swapSelection = [];      // hand indexes chosen for exchange

let pollTimer = null;

// ---------------------------------------------------------------- debug
// Flip to false to silence all console output.
const DEBUG = true;
function dbg(tag, ...args) {
    if (!DEBUG) return;
    const now = new Date();
    const t = now.toLocaleTimeString("ru-RU", { hour12: false }) +
              "." + String(now.getMilliseconds()).padStart(3, "0");
    console.log(
        `%c${t} %c${tag}`,
        "color:#999",
        "color:#4287f5;font-weight:bold",
        ...args
    );
}

// ---------------------------------------------------------------- utils
function qs(id) { return document.getElementById(id); }

let toastTimer = null;
function toast(text, isError = true) {
    const el = qs("toast");
    el.textContent = text;
    el.classList.remove("hidden");
    el.classList.toggle("toast-error", isError);
    el.classList.toggle("toast-ok", !isError);
    clearTimeout(toastTimer);
    toastTimer = setTimeout(() => el.classList.add("hidden"), 2600);
}

function log(text) {
    const list = qs("historyList");
    const p = document.createElement("p");
    p.textContent = text;
    list.appendChild(p);
    list.scrollTop = list.scrollHeight;
}

function setConn(text, cls) {
    const el = qs("connStatus");
    el.textContent = text;
    el.className = "conn-status " + (cls || "");
}

// ---------------------------------------------------------------- ws io
function sendWS(obj) {
    if (!ws || ws.readyState !== WebSocket.OPEN) {
        dbg("WS → BLOCKED (socket not open)", obj, "readyState=", ws && ws.readyState);
        toast("Нет соединения с сервером");
        return;
    }
    dbg("WS →", JSON.stringify(obj));
    ws.send(JSON.stringify(obj));
}

function requestState() { dbg("poll: request state"); sendWS({ action: "state" }); }

// ---------------------------------------------------------------- init
function initGameRoom() {
    if (!token) { dbg("INIT: no token → redirect to login"); window.location.href = "index.html"; return; }

    const params = new URLSearchParams(window.location.search);
    gameId = Number(params.get("game_id"));
    dbg("INIT", { gameId, token: token.slice(0, 8) + "…", WS_URL });
    if (!gameId) { alert("game_id не передан в URL"); return; }

    buildEmptyBoard(15);
    initButtons();
    connect();
}

function connect() {
    dbg("WS: connecting to", WS_URL);
    setConn("Подключение…", "conn-wait");
    ws = new WebSocket(WS_URL);

    ws.onopen = () => {
        // Auth frame first, then ask for the current state.
        const auth = { token: token, game_id: gameId };
        dbg("WS: open → sending auth", { game_id: gameId, token: token.slice(0, 8) + "…" });
        ws.send(JSON.stringify(auth));
        setConn("Онлайн", "conn-ok");
        requestState();
        startPolling();
    };

    ws.onmessage = (event) => {
        let msg;
        try { msg = JSON.parse(event.data); }
        catch (e) { dbg("WS ← UNPARSEABLE", JSON.stringify(event.data)); return; }
        dbg("WS ←", JSON.stringify(msg));
        handleMessage(msg);
    };

    ws.onclose = (event) => {
        dbg("WS: closed", { code: event.code, reason: event.reason, wasClean: event.wasClean });
        stopPolling();
        setConn("Соединение закрыто — обновите страницу", "conn-bad");
        showLobby("Соединение с игрой закрыто.", false);
        disableActions();
    };

    ws.onerror = (e) => { dbg("WS: error", e); setConn("Ошибка соединения", "conn-bad"); };
}

function startPolling() {
    stopPolling();
    // Light poll: keeps the lobby/board fresh even if a push is missed.
    pollTimer = setInterval(requestState, 2500);
}
function stopPolling() { if (pollTimer) { clearInterval(pollTimer); pollTimer = null; } }

// ---------------------------------------------------------------- message handling
//
// Server frames (current protocol):
//   {error: "..."}                                  -> error
//   {ongoing: false}                                -> game not started (lobby)
//   {ongoing: true, public: {...}, private: {...}}  -> full state snapshot
function handleMessage(msg) {
    if (msg.error !== undefined) { dbg("route → error"); handleError(msg.error); return; }
    if (msg.ongoing === false) { dbg("route → lobby (ongoing=false)"); onLobby(); return; }
    if (msg.ongoing === true && msg.public) { dbg("route → state (ongoing=true)"); onState(msg); return; }
    dbg("route → UNHANDLED frame", msg);
}

function onLobby() {
    dbg("LOBBY: game not started");
    started = false;
    myTurn = false;
    showLobby("Игра ещё не началась. Ждём второго игрока и старт.", true);
    disableActions();
}

// Control errors that don't concern the pending placement.
const CONTROL_ERRORS = new Set(["Game has not started", "Not your move"]);

function handleError(err) {
    dbg("ERROR from server:", err);
    if (err === "Game has not started") {
        // Can still arrive if we act before start — treat as lobby, quietly.
        onLobby();
        return;
    }
    toast(err);
    if (CONTROL_ERRORS.has(err)) return;

    // Everything else is a rejected move: the backend reports the specific
    // reason ("Tiles are not in line", "Word does not exist", "Coordinates out
    // of board", "Invalid placement", "Invalid tiles", …). Keep the pending
    // tiles on the board but block submitting them and flag the score invalid.
    setSubmitEnabled(false);
    qs("pendingScore").textContent = "неверно";
}

function onState(msg) {
    lastState = msg;
    if (!started) { log("Игра началась."); }
    started = true;
    hideLobby();

    const prevTurn = myTurn;
    myTurn = Object.prototype.hasOwnProperty.call(msg.private, "pending_score");

    dbg("STATE", {
        current_player: msg.public.current_player,
        players: msg.public.players,
        bag_size: msg.public.bag_size,
        myTurn: myTurn,
        pending_score: msg.private.pending_score,
        hand: msg.private.hand,
        localPending: pending.map(p => `${p.letter}@${p.x},${p.y}`)
    });

    // We can only hold pending tiles during our own turn. Once the turn moves
    // on (we submitted or passed, or it's the opponent's turn) drop the overlay
    // and rely on the authoritative board.
    if (!myTurn) {
        if (pending.length) dbg("STATE: not my turn → clearing local pending");
        pending = [];
        selectedHandIndex = null;
    }

    if (myTurn && !prevTurn) { dbg("turn → MINE"); log("Ваш ход."); }
    if (!myTurn && prevTurn) { dbg("turn → opponent"); }

    renderPlayers(msg.public);
    renderBoard();
    renderHand(msg.private.hand || []);
    updateTurnUI(msg);
}

// ---------------------------------------------------------------- rendering
function buildEmptyBoard(size) {
    const board = qs("gameBoard");
    board.innerHTML = "";
    board.style.display = "grid";
    board.style.gridTemplateColumns = `repeat(${size}, 1fr)`;
    board.style.gridTemplateRows = `repeat(${size}, 1fr)`;

    for (let y = 0; y < size; y++) {
        for (let x = 0; x < size; x++) {
            const cell = document.createElement("div");
            cell.className = "board-cell";
            cell.dataset.x = x;
            cell.dataset.y = y;
            if (x === 7 && y === 7) cell.classList.add("board-center");
            cell.onclick = () => onCellClick(x, y);
            board.appendChild(cell);
        }
    }
}

function pendingAt(x, y) {
    return pending.find(p => p.x === x && p.y === y);
}

function renderBoard() {
    const board = qs("gameBoard");
    const letters = lastState ? lastState.public.letters : null;

    for (const cell of board.children) {
        const x = Number(cell.dataset.x);
        const y = Number(cell.dataset.y);

        const boardLetter = letters ? letters[x][y] : " ";
        const pend = pendingAt(x, y);

        cell.classList.remove("has-tile", "pending-tile");
        if (boardLetter && boardLetter !== " ") {
            cell.textContent = boardLetter;
            cell.classList.add("has-tile");
        } else if (pend) {
            cell.textContent = pend.letter;
            cell.classList.add("pending-tile");
        } else {
            cell.textContent = "";
        }
    }
}

function renderHand(hand) {
    const panel = qs("lettersPanel");
    panel.innerHTML = "";

    const usedIdx = new Set(pending.map(p => p.handIndex));

    hand.forEach((letter, idx) => {
        const tile = document.createElement("div");
        tile.className = "letter-tile";
        tile.textContent = letter;

        if (usedIdx.has(idx)) {
            tile.classList.add("tile-used");
        } else {
            if (selectedHandIndex === idx) tile.classList.add("tile-selected");
            if (swapMode && swapSelection.includes(idx)) tile.classList.add("tile-swap");
            tile.onclick = () => onHandTileClick(idx);
        }
        panel.appendChild(tile);
    });

    const hint = qs("handHint");
    if (swapMode) {
        hint.textContent = "Обмен: выберите фишки и нажмите «Подтвердить обмен».";
    } else if (myTurn) {
        hint.textContent = "Выберите фишку, затем клетку на поле. Клик по своей фишке на поле — вернуть в руку.";
    } else {
        hint.textContent = started ? "Ход соперника…" : "";
    }
}

function renderPlayers(pub) {
    const list = qs("playersList");
    list.innerHTML = "";
    (pub.players || []).forEach((pl, idx) => {
        const div = document.createElement("div");
        div.className = "player-entry";
        if (idx === pub.current_player) div.classList.add("player-current");
        div.textContent = `#${pl.id} — ${pl.score}`;
        if (idx === pub.current_player) div.textContent += "  ⟵ ход";
        list.appendChild(div);
    });
    qs("tilesLeft").textContent = pub.bag_size;
}

function updateTurnUI(msg) {
    const banner = qs("turnBanner");
    if (myTurn) {
        banner.textContent = "— ваш ход";
        banner.className = "turn-banner turn-mine";
    } else {
        banner.textContent = "— ход соперника";
        banner.className = "turn-banner turn-theirs";
    }

    const score = myTurn ? msg.private.pending_score : null;
    if (myTurn && pending.length > 0) {
        qs("pendingScore").textContent = score >= 0 ? String(score) : "неверно";
    } else {
        qs("pendingScore").textContent = "—";
    }

    // Enable/disable action buttons.
    qs("btnPass").disabled = !myTurn || swapMode;
    qs("btnSwap").disabled = !started;               // exchange allowed while game runs
    qs("btnRecall").disabled = pending.length === 0 || swapMode;
    setSubmitEnabled(myTurn && pending.length > 0 && score >= 0);

    // Reflect swap-mode label.
    qs("btnSwap").textContent = swapMode
        ? `Подтвердить обмен (${swapSelection.length})`
        : "Поменять фишки";
}

function setSubmitEnabled(on) { qs("btnSubmit").disabled = !on; }

function disableActions() {
    ["btnSubmit", "btnRecall", "btnPass", "btnSwap"].forEach(id => qs(id).disabled = true);
}

// ---------------------------------------------------------------- interactions
function onHandTileClick(idx) {
    dbg("click hand tile", idx, "letter=", lastState && lastState.private.hand[idx], "swapMode=", swapMode);
    if (swapMode) {
        const at = swapSelection.indexOf(idx);
        if (at >= 0) swapSelection.splice(at, 1);
        else swapSelection.push(idx);
        dbg("swap selection now", swapSelection);
        renderHand(lastState.private.hand);
        qs("btnSwap").textContent = `Подтвердить обмен (${swapSelection.length})`;
        return;
    }
    if (!myTurn) { dbg("hand click ignored: not my turn"); toast("Сейчас не ваш ход"); return; }
    selectedHandIndex = (selectedHandIndex === idx) ? null : idx;
    dbg("selectedHandIndex =", selectedHandIndex);
    renderHand(lastState.private.hand);
}

function onCellClick(x, y) {
    dbg("click cell", { x, y, selectedHandIndex, myTurn, swapMode });
    if (swapMode) return;
    if (!started) return;

    const pend = pendingAt(x, y);
    if (pend) {                       // recall a pending tile back to the hand
        dbg("recall pending tile", pend.letter, "from", x, y);
        pending = pending.filter(p => !(p.x === x && p.y === y));
        syncPlacement();
        return;
    }

    if (!myTurn) { toast("Сейчас не ваш ход"); return; }
    if (selectedHandIndex === null) { toast("Сначала выберите фишку"); return; }

    const boardLetter = lastState.public.letters[x][y];
    if (boardLetter && boardLetter !== " ") { dbg("cell occupied", x, y, "=", boardLetter); toast("Клетка занята"); return; }

    const letter = lastState.private.hand[selectedHandIndex];
    pending.push({ x, y, letter, handIndex: selectedHandIndex });
    dbg("place pending tile", letter, "at", x, y);
    selectedHandIndex = null;
    syncPlacement();
}

// Push the current pending set to the server for validation / scoring.
function syncPlacement() {
    renderBoard();
    renderHand(lastState.private.hand);
    qs("btnRecall").disabled = pending.length === 0;

    if (pending.length === 0) {
        dbg("syncPlacement: pending empty → submit disabled");
        setSubmitEnabled(false);
        qs("pendingScore").textContent = "—";
        return;
    }
    dbg("syncPlacement: sending place for", pending.map(p => `${p.letter}@${p.x},${p.y}`));
    sendWS({
        action: "place",
        coordinates: pending.map(p => [p.x, p.y]),
        letters: pending.map(p => p.letter).join("")
    });
}

// ---------------------------------------------------------------- buttons
function initButtons() {
    qs("btnStart").onclick = startGame;

    qs("btnSubmit").onclick = () => {
        if (qs("btnSubmit").disabled) return;
        dbg("btn: submit");
        sendWS({ action: "submit" });
        log("Вы отправили слово.");
    };

    qs("btnRecall").onclick = () => {
        dbg("btn: recall all pending");
        pending = [];
        selectedHandIndex = null;
        syncPlacement();
    };

    qs("btnPass").onclick = () => {
        if (!myTurn) { toast("Сейчас не ваш ход"); return; }
        dbg("btn: pass");
        pending = [];
        sendWS({ action: "pass" });
        log("Вы спасовали.");
    };

    qs("btnSwap").onclick = () => {
        dbg("btn: swap", { swapMode, swapSelection });
        if (!swapMode) {
            swapMode = true;
            swapSelection = [];
            selectedHandIndex = null;
            renderHand(lastState ? lastState.private.hand : []);
            updateTurnUI(lastState || { public: {}, private: {} });
            return;
        }
        // Confirm exchange.
        if (swapSelection.length === 0) { exitSwapMode(); return; }
        const tiles = swapSelection.map(i => lastState.private.hand[i]).join("");
        sendWS({ action: "change", tiles: tiles });
        log("Обмен фишек: " + tiles);
        exitSwapMode();
    };
}

function exitSwapMode() {
    swapMode = false;
    swapSelection = [];
    if (lastState) { renderHand(lastState.private.hand); updateTurnUI(lastState); }
    else qs("btnSwap").textContent = "Поменять фишки";
}

async function startGame() {
    dbg("HTTP → POST /game", { action: "start", game_id: gameId });
    qs("btnStart").disabled = true;
    try {
        const resp = await apiPost("/game", { action: "start", token: token, game_id: gameId });
        dbg("HTTP ← /game start OK:", resp);
        showLobby("Запуск…", false);
        requestState();
    } catch (err) {
        dbg("HTTP ← /game start FAILED:", err.message);
        toast("Не удалось начать игру: " + err.message);
        qs("btnStart").disabled = false;
    }
}

// ---------------------------------------------------------------- lobby
function showLobby(message, showStart) {
    qs("lobbyBox").classList.remove("hidden");
    qs("lobbyMsg").textContent = message;
    const btn = qs("btnStart");
    btn.classList.toggle("hidden", !showStart);
    btn.disabled = false;
}
function hideLobby() { qs("lobbyBox").classList.add("hidden"); }

window.onload = initGameRoom;
