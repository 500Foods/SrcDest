#!/usr/bin/env python3

# agentp.py
#
# Typically deployed as /usr/local/bin/AgentP
# Typically als uses /usr/local/etc/Agent.json for its configuration


import sys
import json
import asyncio
import websockets
import os.path

from prompt_toolkit import PromptSession
from prompt_toolkit.history import InMemoryHistory
from prompt_toolkit.patch_stdout import patch_stdout


async def read_input(input_queue, history, stop_event):
    session = PromptSession(history=history)

    while not stop_event.is_set():
        try:
            with patch_stdout():
                user_input = await session.prompt_async("Enter JSON to send (Ctrl+C to quit): ")
            await input_queue.put(user_input)
        except (EOFError, KeyboardInterrupt):
            await input_queue.put(None)
            break


async def send_json(websocket, json_data):
    await websocket.send(json.dumps(json_data))


async def receive_json(websocket):
    response = await websocket.recv()
    print("Received JSON:", json.dumps(json.loads(response), indent=2))


async def check_connection(websocket, stop_event):
    while not stop_event.is_set():
        try:
            # Ping/Pong so we now our connection is still active
            await asyncio.wait_for(websocket.ping(), timeout=5)
            await asyncio.sleep(5)  

        except asyncio.TimeoutError:
            print("Lost connection to the WebSocket server")
            stop_event.set()
            break

        except websockets.exceptions.ConnectionClosed:
            print("Ended connection to the WebSocket server")
            stop_event.set()
            break


async def main():

    # Parameters <config.json> <remote-system>
    config_file = sys.argv[1]
    remote_system = sys.argv[2]
    remote_url = ''
    remote_protocol = ''
    remote_port = 0

    # Load configuration
    with open(config_file, 'r') as file:
        config = json.load(file)

    # Must have Agent Systems defined
    if not('Agent Systems' in config):
        print("No Agent Systems found in config")
        exit()

    # Find the remote-system in list of Agent Systems
    agent_systems = config['Agent Systems']
    for agent_system in agent_systems:
        if remote_system in agent_system:
            remote_config = agent_system[remote_system]
            if 'URL' in remote_config:
                remote_url = remote_config['URL']
            if 'Protocol' in remote_config:
                remote_protocol = remote_config['Protocol']
            break

    # Must have remote-system in Agent Systems
    if remote_url == '':
        print(f"Remote system [{remote_system}] not found in config")
        exit()

    # To connect, remote-system must have a URL like wss://example.com/ws
    if not(remote_url.startswith('wss://')):
        print(f"URL must refer to a websocket (wss://) resource [{remote_url}]")
        exit()

    # Extra check that if running locally, lets connect to it directly
    if 'Agent Server' in config:
        agent_server = config['Agent Server']
        if 'Name' in agent_server: 
            agent_name = agent_server['Name']
            if agent_name == remote_system:
                if 'Port' in agent_server:
                    remote_port = agent_server['Port']
                if 'Protocol' in agent_server:
                    remote_protocol = agent_server['Port']

    # Configure local or remote connection string
    if remote_port != 0:
        connection = f"ws://localhost:{remote_port}"
    else:
        connection = remote_url

    # This is the command history so we don't have to type it all the time
    history_data = config.get('Agent History', [])
    history = InMemoryHistory(history_data)

    stop_event = asyncio.Event()

    try:
        print(f"Establishing server connection to {connection}")
        async with websockets.connect(connection, subprotocols=[remote_protocol]) as websocket:
            print(f"Server connection established")

            input_queue = asyncio.Queue()
            input_task = asyncio.create_task(read_input(input_queue, history, stop_event))
            connection_check_task = asyncio.create_task(check_connection(websocket, stop_event))

            while not stop_event.is_set():
                user_input_task = asyncio.create_task(input_queue.get())
                done, _ = await asyncio.wait({user_input_task, connection_check_task}, return_when=asyncio.FIRST_COMPLETED)

                if connection_check_task in done:
                    break

                user_input = user_input_task.result()
                if user_input is None:
                    break

                try:
                    json_data = json.loads(user_input)
                    if user_input not in history_data:
                        history_data.append(user_input)
                        config['Client History'] = history_data
                        with open(config_file, 'w') as file:
                            json.dump(config, file, indent=2)

                    await send_json(websocket, json_data)
                    await receive_json(websocket)
                except json.JSONDecodeError:
                    if user_input:
                        print("[Invalid JSON]")

            input_task.cancel()
            connection_check_task.cancel()
            try:
                await input_task
                await connection_check_task
            except asyncio.CancelledError:
                pass
            await websocket.close()

    except (OSError, websockets.exceptions.InvalidHandshake, websockets.exceptions.InvalidMessage) as e:
        print("Failed to connect to the server")


if __name__ == '__main__':

    if len(sys.argv) != 3:
        print("Usage: AgentP <config.json> <remote-system>")
        exit()
  
    if os.path.isfile(sys.argv[1]) == False:
        print("Configuration file not found")
        exit()

    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
    finally:
        print("Agent exited")

