# API

## /login
expects:
```jsonc
{
    "login": "login",
    "passwd": "passwd"
}
```
returns
```jsonc
[
    "UserNotExists" // if user not exists
    "PasswdIncorrect" // if password is wrong
    "InternalFailure" 
    "raw_token_for_client" // if login is success returns raw token
]
```
## /reg
expects:
```jsonc
{
    "email": "email@domen.com",
    "passwd": "passwd",
    "nick": "nickname" //
}
```
returns
```jsonc
[
    NicknameExists
    UserExists
    InternalFailure
    OK
]
```
## /game
expects:
```jsonc
"enumAction": [
    "join",
    "start",
    "end",
    "create",
    "list"
]
{
    "action": "enumAction action",
    "token": "user_token"
    "game_id": "game id" // if starting|joining|ending a game
}
```
returns:
```jsonc
// if "action": "create"
"new_game_id"
// if "action": "join"
"game_id" || "NotJoined"
// if "action": "start"
game_id ||
// if "action": "end"
game_id || ""
// if "action": "list"
{
    "game_info_list": [
        {
            "game_id": "id",
            "host_user_name": "name",
            "num_of_users": 1234
        },
    ]
}
```

## /ws
expects: 
```jsonc
{
}
```

