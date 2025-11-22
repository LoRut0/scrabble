# notes

```sh
redis-cli -p 6379 to connect to redis client
keys * to list all
get <key>
```

```
sqlite3 /tmp/keyjson.db # to connect to sqlite3
select * from ongoing
```

## Initializing a game

1. start game [x]
2. ScrabbleGame() init [x]
    1. board dimensions
    2. board bonus squares
    3. FillBag_() [x]
    4. PlayerState() init [x]
        1. DrawTile() [x]
        2. FillHand() [x]
3. FINALLY game starts [?]

## Course of the Game

1. player places tile [x]
2. TryPlaceTiles() [x]
    1. new_tiles_coordinates filled [x]
    2. new_tiles become filled [x]
3. if True, player can submit word [x]
4. SubmitWord() [x]
5. new_board receives new_tiles, new_coords [x]
