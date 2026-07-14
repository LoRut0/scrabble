import json
import pytest


EMAIL = 'testuser@example.com'
NICK = 'testuser'
NICK2 = 'LoRuto'
PASSWD = '1234'


@pytest.fixture
async def token(service_client):
    # регистрация (может уже существовать — не проверяем статус)
    await service_client.post('/reg', json={
        'email': EMAIL,
        'passwd': PASSWD,
        'nick': NICK,
    })

    resp = await service_client.post('/login', json={
        'login': NICK,
        'passwd': PASSWD,
    })
    assert resp.status == 200
    return resp.text


@pytest.fixture
async def game_id(service_client, token):
    resp = await service_client.post('/game', json={
        'token': token,
        'action': 'create',
    })
    assert resp.status == 200
    return int(resp.text)


async def test_register(service_client):
    resp = await service_client.post('/reg', json={
        'email': 'new@example.com',
        'passwd': 'somepass',
        'nick': 'newuser',
    })
    assert resp.status == 200


async def test_login_wrong_password(service_client):
    resp = await service_client.post('/login', json={
        'login': NICK,
        'passwd': 'wrongpassword',
    })
    assert resp.status == 400


async def test_list_games(service_client, token):
    resp = await service_client.post('/game', json={
        'token': token,
        'action': 'list',
    })
    assert resp.status == 200
    data = resp.json()
    assert 'game_info_list' in data


async def test_create_and_join_game(service_client, token, game_id):
    resp = await service_client.post('/game', json={
        'token': token,
        'action': 'join',
        'game_id': game_id,
    })
    assert resp.status == 200
    assert int(resp.text) == game_id


async def test_websocket_auth(service_client, websocket_client, token, game_id):
    # join
    await service_client.post('/game', json={
        'token': token,
        'action': 'join',
        'game_id': game_id,
    })

    # second player registers and joins (game needs 2 players to start)
    await service_client.post('/reg', json={
        'email': 'testuser2@example.com',
        'passwd': PASSWD,
        'nick': NICK2,
    })
    resp = await service_client.post('/login', json={
        'login':  NICK2,
        'passwd': PASSWD,
    })
    assert resp.status == 200
    token2 = resp.text

    resp = await service_client.post('/game', json={
        'token': token2,
        'action': 'join',
        'game_id': game_id,
    })
    assert resp.status == 200

    resp = await service_client.post('/game', json={
        'token': token,
        'action': 'start',
        'game_id': game_id,
    })
    assert resp.status == 200

    async with websocket_client.get('ws') as ws:
        # first message - auth
        await ws.send(json.dumps({
            'token': token,
            'game_id': game_id,
        }))

        await ws.send(json.dumps({'action': 'state'}))

        msg = await ws.recv()
        data = json.loads(msg)
        assert 'public' in data
        assert 'private' in data
        assert 'hand' in data['private']
