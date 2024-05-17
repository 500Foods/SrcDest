import sys
import json
import asyncio
import websockets
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
            await asyncio.wait_for(websocket.ping(), timeout=10)
            await asyncio.sleep(5)  # Adjust the interval as needed
        except asyncio.TimeoutError:
            print("Lost connection to the WebSocket server.")
            stop_event.set()
            break
        except websockets.exceptions.ConnectionClosed:
            print("Lost connection to the WebSocket server.")
            stop_event.set()
            break

async def main():
    config_file = sys.argv[1]
    with open(config_file, 'r') as file:
        config = json.load(file)

    agent_port = config['Agent Websocket']

    history_data = config.get('Client History', [])
    history = InMemoryHistory(history_data)

    stop_event = asyncio.Event()

    try:
        async with websockets.connect(f"ws://localhost:{agent_port}", subprotocols=['agentc-protocol']) as websocket:
            print(f"Connected to WebSocket server on ws://localhost:{agent_port}")

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
        print("Failed to connect to the server. Please check if the server is running and accessible.")

if __name__ == '__main__':
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
    finally:
        print("Program exited gracefully.")

