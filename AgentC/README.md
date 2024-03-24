# AgentC

This is a relatively simple WebSocket server written in C (hence the clever name). Its purpose is to run on whatever systems need to be synchronized (sources or destinations). 

Its main purpose is to monitor the local filesystem for changes and to be able to report on those changes to any authorized request. 
Beyond that, it can also provide other information like the current status of itself, event history, and so on, like any good secret agent should be able to do. 

This was written in C primarily to ensure that it was as performant and as light a touch as possible for any system it is running on. We live in an age where systems
typically have dozens of these kinds of agents running amok, and while contributing to the clutter is likely unavoidable, we can at least try and be polite about it.

## Configuration
A sample JSON configuration file has been provided, showing the key bits that are needed to get this up and running.
This would typically be customized for each system (the Agent Server part) while each such system would also be listed (the Agent Systems part).
this is also shared with the Python client (see below), which stores its history here as well, just as a convenience.

## AgentP
Having a shiny new C-based WebSocket server is great and all, but not of much use out of the gate unless we can test it. 
To help with that, a Python script called AgentP has been provided. It can be pointed at the same JSON configuration file, and then 
selecting a system to connect to will give you the ability to send and receive messages (JSON). 

## Dependencies
While we're not doing anything particularly interesting here, we do need a few extra bits.
- libwebsockets - The websockets interface, naturally
- jansson - A C library for handling JSON
- fswatch - To monitor OS-level file changes in a multiplatform manner
- xxhash - To generate file hashes, very quickly, so we can tell what has changed
- sqlite3 - For local storage of file data

