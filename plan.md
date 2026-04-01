WebsocketHandler
    -> authenticates user
    -> finds GameRoom
    -> creates PlayerSession
    -> registers session in GameRoom
    -> reads incoming client messages
    -> forwards commands to GameRoom
    -> runs outgoing send loop for this session

GameRoom
    -> owns ScrabbleGame
    -> owns active sessions/subscribers
    -> applies commands
    -> after each change builds views and broadcasts

ScrabbleGame
    -> pure rules and state

GameViewBuilder
    -> converts full game state into public/private json

GameStorage / GameManager
    -> map<game_id, shared_ptr<GameRoom>>
