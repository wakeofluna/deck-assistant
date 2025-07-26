#!/usr/bin/python3

import aiohttp
from aiohttp import web, WSCloseCode
import asyncio
import json
import random
import datetime
from pathlib import Path


PREDEFINED_FILE = 'fake_twitch_messages.json'
HOST = 'localhost'
PORT = 8080

connected_clients = set()
predefined_messages = []


def load_predefined_messages():
    global predefined_messages
    path = Path(PREDEFINED_FILE)
    try:
        with path.open('rb') as f:
            predefined_messages = json.loads(f.read())
        print(f'Reloaded {len(predefined_messages)} messages')
        return True, len(predefined_messages)
    except Exception as e:
        print(f'Failed to reload messages: {e}')
        return False, str(e)


def wrap_payload_in_message(message_type: str, payload: dict):
    if 'metadata' in payload:
        return payload

    msg = {
        'metadata': {
            'message_id': random.randbytes(10).hex(),
            'message_type': message_type,
            'message_timestamp': datetime.datetime.today().isoformat(),
        },
        'payload': payload,
    }

    if message_type == 'notification':
        msg['metadata']['subscription_type'] = payload['subscription']['type']
        msg['metadata']['subscription_version'] = 1

    return msg


async def broadcast(payload: dict):
    if connected_clients:
        message = wrap_payload_in_message('notification', payload)
        message_str = json.dumps(message)
        await asyncio.gather(*(client.send_str(message_str) for client in connected_clients))


async def root_handler(request):
    return web.json_response({ 'ok':True, 'message':'Fake Twitch Server reporting in' })


async def websocket_handler(request):
    ws = web.WebSocketResponse()
    await ws.prepare(request)

    connected_clients.add(ws)
    try:
        welcome = {
            'session': {
                'id': random.randbytes(10).hex(),
                'status': 'connected',
                'keepalive_timeout_seconds': 30,
            }
        }
        welcome = wrap_payload_in_message('session_welcome', welcome)
        await ws.send_json(welcome)

        async for msg in ws:
            if msg.type == aiohttp.WSMsgType.TEXT:
                pass
            elif msg.type == aiohttp.WSMsgType.ERROR:
                print('WebSocket error %s' % ws.exception())
    finally:
        print('WebSocket connection closed')
        connected_clients.remove(ws)
        await ws.close()

    return ws


async def reload_handler(request):
    ok, count = load_predefined_messages()
    return web.json_response({ 'ok':ok, 'count':count })


async def list_handler(request):
    messages = [k for k in predefined_messages]
    return web.json_response(messages)


async def write_handler(request):
    try:
        data = await request.json()
        await broadcast(data)
        return web.json_response({ 'ok':True })
    except Exception as e:
        return web.json_response({ 'ok':False, 'error':f'Invalid JSON: {e}'}, status=400)


async def write_msg_handler(request):
    param = request.match_info.get('id', None)
    data = predefined_messages.get(param)
    if data:
        await broadcast(data)
        return web.json_response({ 'ok':True })
    else:
        return web.json_response({ 'ok':False, 'error':f'No message with id {param}' }, status=400)


async def eventsub_handler(request):
    try:
        data = await request.json()
        typ = data.get('type')
        version = data.get('version')
        condition = data.get('condition')
        print(f'Got SUBSCRIBE for {typ} version {version} with condition {json.dumps(condition)}')

        new_id = random.randint(100000, 999999)
        return web.json_response({ 'data': [{ 'id': new_id, 'type': typ, 'condition': condition },]})
    except Exception as e:
        return web.json_response({ 'message':f'Invalid JSON: {e}'}, status=400)


def create_runner():
    app = web.Application()
    app.add_routes([
        web.get('/', root_handler),
        web.get('/ws', websocket_handler),
        web.get('/reload', reload_handler),
        web.get('/list', list_handler),
        web.post('/write', write_handler),
        web.get(r'/write/{id:\w+}', write_msg_handler),
        web.post('/eventsub/subscriptions', eventsub_handler),
    ])
    return web.AppRunner(app)


async def start_server(host=HOST, port=PORT):
    runner = create_runner()
    await runner.setup()
    site = web.TCPSite(runner, host, port)
    await site.start()
    print(f'Hosting server at http://{host}:{port}')


if __name__ == "__main__":
    load_predefined_messages()
    loop = asyncio.new_event_loop()
    loop.run_until_complete(start_server())
    loop.run_forever()
