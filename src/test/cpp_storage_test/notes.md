# notes

- receives json
  json like:

```
{
    "id": int,
    "action": string
}
```

id is id of game
action is:

- increment (++1 to value of game with given id)
- get (get value of game with given id)
- new (should make new game with random id, return that id)
- all (should return all games that exist)
