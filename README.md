# Scrabble Online (C++ + userver)

Онлайн-игра **Scrabble** с поддержкой многопользовательского режима, реализованная на:

- **C++**
- **userver framework**
- **WebSocket**
- **SQLite**
- HTML / CSS / JavaScript (frontend)

Проект включает авторизацию, лобби игр, игровую комнату с историей ходов и синхронизацию в реальном времени.

---

## Возможности

- Регистрация и вход пользователя
- Создание и подключение к играм
- Поддержка нескольких игроков
- История ходов
- Обновления в реальном времени через WebSocket
- Полная игровая логика Scrabble на C++
- Хранение данных в SQLite
- Redis для хранения состояния игры и pub/sub
- Веб-интерфейс

---

## Структура проекта

### Backend (C++ / userver)

- `main.cpp` — точка входа приложения
- `ScrabbleGame.hpp / ScrabbleGame.cpp` — логика игры
- `handlers.hpp` — объявления HTTP-обработчиков
- `http.cpp` — REST API
- `websocket.cpp` — обработчик WebSocket
- `sqlite.cpp` — работа с SQLite
- `Cors.hpp / Cors.cpp` — поддержка CORS
- `utils.hpp` — вспомогательные функции
- `static_config.yaml` — конфигурация userver

---

### Frontend

- `index.html` — страница входа и регистрации
- `game_list.html` — список игр (лобби)
- `game_room.html` — игровая комната
- `styles.css` — стили

---

### Структура проекта

.
├── Makefile
├── README.md
├── src
│   ├── frontend
│   │   ├── game_list.html
│   │   ├── game_room.html
│   │   ├── game_room_old.html
│   │   ├── index.html
│   │   ├── js
│   │   │   ├── api.js
│   │   │   ├── game_room.js
│   │   │   ├── games.js
│   │   │   ├── login.js
│   │   │   ├── register.js
│   │   │   └── user_info.js
│   │   ├── profile.html
│   │   └── styles.css
│   ├── sqlite
│   │   ├── users_1.sql
│   └── userver
│   ├── CMakeLists.txt
│   ├── Cors.cpp
│   ├── Cors.hpp
│   ├── DiceGame.cpp
│   ├── DiceGame.hpp
│   ├── example_for_scrabble.json
│   ├── example.json
│   ├── Findsodium.cmake
│   ├── handlers.hpp
│   ├── http.cpp
│   ├── main.cpp
│   ├── plan.md
│   ├── redis.cpp
│   ├── ScrabbleGame.cpp
│   ├── ScrabbleGame.hpp
│   ├── sqlite.cpp
│   ├── utils.hpp
│   └── websocket.cpp
└── static_config.yaml
