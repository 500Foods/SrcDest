#!/usr/bin/env python3

import sys
import json
import asyncio
import websockets
import os.path
import tkinter as tk
from tkinter import ttk
from tkinter import scrolledtext

class AgentGUI:
    def __init__(self, config_file):
        self.config_file = config_file
        self.config = self.load_config()
        self.history_data = self.config.get('Agent History', [])
        self.websocket = None
        self.stop_event = asyncio.Event()
        self.version = "1.0.14"  # Change this version number whenever the source code is modified

    def load_config(self):
        with open(self.config_file, 'r') as file:
            return json.load(file)

    def save_config(self):
        with open(self.config_file, 'w') as file:
            json.dump(self.config, file, indent=2)

    async def choose_system(self):
        root = tk.Tk()
        root.title(f"AgentU v{self.version}: Select System")

        screen_width = root.winfo_screenwidth()
        screen_height = root.winfo_screenheight()

        # Load the window size and position from the configuration or set default values
        window_size = self.config.get("Agent UI", {}).get("System Selector Window", {"width": 400, "height": 400})
        window_pos = self.config.get("Agent UI", {}).get("System Selector Window", {"x": (screen_width - window_size["width"]) // 2, "y": (screen_height - window_size["height"]) // 2})

        root.geometry(f"{window_size['width']}x{window_size['height']}+{window_pos['x']}+{window_pos['y']}")
        root.resizable(True, True)
        root.minsize(240, max(240, len(self.config['Agent Systems']) * 50))

        frame = ttk.Frame(root)
        frame.pack(fill=tk.BOTH, expand=True)

        button_frame = ttk.Frame(frame)
        button_frame.place(relx=0.5, rely=0.5, anchor=tk.CENTER)

        def on_button_click(system):
            root.destroy()
            asyncio.ensure_future(self.start_chat_interface(system))

        for system in self.config['Agent Systems']:
            button = ttk.Button(button_frame, text=list(system.keys())[0], width=20, command=lambda s=list(system.keys())[0]: on_button_click(s))
            button.pack(pady=10)

        def on_window_resize(event):
            button_frame.place_configure(relx=0.5, rely=0.5, anchor=tk.CENTER)
            if "Agent UI" not in self.config:
                self.config["Agent UI"] = {}
            self.config["Agent UI"]["System Selector Window"] = {"width": event.width, "height": event.height, "x": root.winfo_x(), "y": root.winfo_y()}
            self.save_config()

        root.bind("<Configure>", on_window_resize)

        def on_close():
            self.stop_event.set()
            root.destroy()

        root.protocol("WM_DELETE_WINDOW", on_close)

        while not self.stop_event.is_set():
            root.update()
            await asyncio.sleep(0.01)

    async def start_chat_interface(self, selected_system):
        root = tk.Tk()
        root.title(f"Chat with {selected_system}")

        screen_width = root.winfo_screenwidth()
        screen_height = root.winfo_screenheight()

        # Load the window size and position from the configuration or set default values
        window_size = self.config.get("Agent UI", {}).get("Chat Window", {"width": 600, "height": 600})
        window_pos = self.config.get("Agent UI", {}).get("Chat Window", {"x": (screen_width - window_size["width"]) // 2, "y": (screen_height - window_size["height"]) // 2})

        root.geometry(f"{window_size['width']}x{window_size['height']}+{window_pos['x']}+{window_pos['y']}")
        root.resizable(True, True)
        root.minsize(400, 400)

        chat_history = scrolledtext.ScrolledText(root, state=tk.DISABLED)
        chat_history.pack(padx=10, pady=10, expand=True, fill=tk.BOTH)

        chat_history.tag_configure("sent", foreground="blue")
        chat_history.tag_configure("received", foreground="green")
        chat_history.tag_configure("error", foreground="red")
        chat_history.tag_configure("info", foreground="purple")

        chat_history.configure(state=tk.NORMAL)
        chat_history.insert(tk.END, "Initialization complete.\n", "info")
        chat_history.configure(state=tk.DISABLED)
        chat_history.see(tk.END)

        input_frame = ttk.Frame(root)
        input_frame.pack(padx=10, pady=5, fill=tk.X)

        dropdown = ttk.Combobox(input_frame, values=self.history_data, state="readonly", width=20)
        dropdown.pack(side=tk.LEFT, padx=(0, 10))

        def on_dropdown_select(event):
            selected_message = dropdown.get()
            input_entry.delete("1.0", tk.END)
            input_entry.insert("1.0", selected_message)

        dropdown.bind("<<ComboboxSelected>>", on_dropdown_select)

        input_entry = tk.Text(input_frame, height=3)
        input_entry.pack(side=tk.LEFT, fill=tk.X, expand=True)

        submit_button = ttk.Button(input_frame, text="Submit", command=lambda: asyncio.ensure_future(send_message()), width=10)
        submit_button.pack(side=tk.RIGHT, padx=(10, 0))

        def format_json(json_text):
            try:
                json_data = json.loads(json_text)
                formatted_json = json.dumps(json_data, indent=2)
                return formatted_json
            except json.JSONDecodeError:
                return json_text

        async def send_message():
            message = input_entry.get("1.0", tk.END).strip()
            if message:
                submit_button.config(text="Sending...", state=tk.DISABLED)
                input_entry.config(state=tk.DISABLED)
                dropdown.config(state=tk.DISABLED)
        
                try:
                    json_data = json.loads(message)
                    if message not in self.history_data:
                        self.history_data.append(message)
                        self.config['Agent History'] = self.history_data
                        self.save_config()
                        dropdown['values'] = self.history_data
        
                    await self.send_json(json_data)
                    response = await self.receive_json()
        
                    chat_history.configure(state=tk.NORMAL)
                    chat_history.insert(tk.END, f"Sent:\n{format_json(message)}\n\n", "sent")
                    chat_history.insert(tk.END, f"Received:\n{format_json(json.dumps(response))}\n\n", "received")
                    chat_history.configure(state=tk.DISABLED)
                    chat_history.see(tk.END)
                except json.JSONDecodeError:
                    chat_history.configure(state=tk.NORMAL)
                    chat_history.insert(tk.END, "[Invalid JSON]\n", "error")
                    chat_history.configure(state=tk.DISABLED)
                    chat_history.see(tk.END)
        
                input_entry.delete("1.0", tk.END)
                submit_button.config(text="Submit", state=tk.NORMAL)
                input_entry.config(state=tk.NORMAL)
                dropdown.config(state="readonly")
                
        async def start_connection():
            remote_system = selected_system
            remote_config = None
            for agent_system in self.config['Agent Systems']:
                if remote_system in agent_system:
                    remote_config = agent_system[remote_system]
                    break

            if remote_config:
                remote_url = remote_config.get('URL', '')
                remote_protocol = remote_config.get('Protocol', '')
                remote_port = 0

                print(f"Remote URL: {remote_url}")  # Debug message
                print(f"Remote Protocol: {remote_protocol}")  # Debug message

                if 'Agent Server' in self.config:
                    agent_server = self.config['Agent Server']
                    if 'Name' in agent_server and agent_server['Name'] == remote_system:
                        remote_port = agent_server.get('Port', 0)
                        remote_protocol = agent_server.get('Protocol', '')

                print(f"Remote Port: {remote_port}")  # Debug message

                if remote_port != 0:
                    connection = f"ws://localhost:{remote_port}"
                else:
                    connection = remote_url

                print(f"Connection URL: {connection}")  # Debug message

                chat_history.configure(state=tk.NORMAL)
                chat_history.insert(tk.END, f"Attempting connection to {connection}...\n", "info")
                chat_history.configure(state=tk.DISABLED)
                chat_history.see(tk.END)

                max_retries = 3
                retry_delay = 5  # seconds

                for retry in range(max_retries):
                    try:
                        print(f"Connection attempt {retry + 1}")  # Debug message
                        async with websockets.connect(connection, subprotocols=[remote_protocol]) as websocket:
                            print("Connected to the WebSocket server")  # Debug message
                            self.websocket = websocket
                            chat_history.configure(state=tk.NORMAL)
                            chat_history.insert(tk.END, "Connected. Ready for communication.\n", "info")
                            chat_history.configure(state=tk.DISABLED)
                            chat_history.see(tk.END)

                            while not self.stop_event.is_set():
                                try:
                                    await asyncio.wait_for(websocket.ping(), timeout=5)
                                    await asyncio.sleep(5)
                                except asyncio.TimeoutError:
                                    chat_history.configure(state=tk.NORMAL)
                                    chat_history.insert(tk.END, "Lost connection to the WebSocket server.\n", "error")
                                    chat_history.configure(state=tk.DISABLED)
                                    chat_history.see(tk.END)
                                    self.stop_event.set()
                                    break
                                except websockets.exceptions.ConnectionClosed:
                                    chat_history.configure(state=tk.NORMAL)
                                    chat_history.insert(tk.END, "Ended connection to the WebSocket server.\n", "error")
                                    chat_history.configure(state=tk.DISABLED)
                                    chat_history.see(tk.END)
                                    self.stop_event.set()
                                    break

                            return  # Exit the function if the connection is successful

                    except (OSError, websockets.exceptions.InvalidHandshake, websockets.exceptions.InvalidMessage) as e:
                        print(f"Error while connecting to the server: {str(e)}")  # Debug message
                        chat_history.configure(state=tk.NORMAL)
                        chat_history.insert(tk.END, f"Connection attempt {retry + 1} failed.\n", "error")
                        chat_history.configure(state=tk.DISABLED)
                        chat_history.see(tk.END)

                        if retry < max_retries - 1:
                            chat_history.configure(state=tk.NORMAL)
                            chat_history.insert(tk.END, f"Retrying in {retry_delay} seconds...\n", "info")
                            chat_history.configure(state=tk.DISABLED)
                            chat_history.see(tk.END)
                            await asyncio.sleep(retry_delay)
                        else:
                            chat_history.configure(state=tk.NORMAL)
                            chat_history.insert(tk.END, "Failed to establish connection.\n", "error")
                            chat_history.configure(state=tk.DISABLED)
                            chat_history.see(tk.END)
                            return  # Exit the function if all retries fail

                    except Exception as e:
                        print(f"Unexpected error: {str(e)}")  # Debug message
                        chat_history.configure(state=tk.NORMAL)
                        chat_history.insert(tk.END, f"Unexpected error: {str(e)}\n", "error")
                        chat_history.configure(state=tk.DISABLED)
                        chat_history.see(tk.END)
                        return  # Exit the function if an unexpected error occurs

            else:
                print("Remote system configuration not found")  # Debug message
                chat_history.configure(state=tk.NORMAL)
                chat_history.insert(tk.END, "Remote system configuration not found.\n", "error")
                chat_history.configure(state=tk.DISABLED)
                chat_history.see(tk.END)

        def on_window_resize(event):
            if "Agent UI" not in self.config:
                self.config["Agent UI"] = {}
            self.config["Agent UI"]["Chat Window"] = {"width": event.width, "height": event.height, "x": root.winfo_x(), "y": root.winfo_y()}
            self.save_config()

        def on_close():
            self.stop_event.set()
            if self.websocket:
                asyncio.ensure_future(self.websocket.close())
            root.destroy()

        root.bind("<Configure>", on_window_resize)
        root.protocol("WM_DELETE_WINDOW", on_close)

        asyncio.ensure_future(start_connection())

        while not self.stop_event.is_set():
            root.update()
            await asyncio.sleep(0.01)

    async def send_json(self, json_data):
        await self.websocket.send(json.dumps(json_data))

    async def receive_json(self):
        response = await self.websocket.recv()
        return json.loads(response)

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print("Usage: AgentP <config.json>")
        exit()

    config_file = sys.argv[1]

    if not os.path.isfile(config_file):
        print("Configuration file not found")
        exit()

    agent_gui = AgentGUI(config_file)
    try:
        asyncio.run(agent_gui.choose_system())
    except KeyboardInterrupt:
        print("Program interrupted by user.")
        agent_gui.stop_event.set()
