# AgentC

This is a relatively simple WebSocket server written in C (hence the clever name). Its purpose is to run on whatever systems need to be synchronized (sources or destinations). 
While it is still early in the development cycle, the expectation is that this will run on various reasonably current Linux, macOS and Windows systems. As a C program, some
effort has been spent trying to ensure as few dependencies as possible are required, and when they are, that they work in these environments. 

The agent's main purpose is to monitor the local filesystem for changes and to be able to report on those changes to any authorized request. Beyond that, it can also provide 
other information like the current status of itself, local event history, and so on, like any good secret agent should be able to do.

This was written in C primarily to ensure that it was as performant and as light a touch as possible for any system it is running on. We live in an age where systems
typically have dozens of these kinds of agents running amok, and while contributing to the clutter is likely unavoidable, we can at least try and be polite about it.

## REST API Proxy
A pet peeve of mine is when projects of various flavors and complexities offer up different pathways to their data, such as with a REST API and also a WebSockets API.
Or perhaps its a REST API and a GraphQL API. But they offer up pathways that are then all required, as some data is available via one pathway, and other data via 
another. So instead of building one client for a service, we have to build multiple. With that in mind, this project has been equipped with a REST API proxy facility.
This means that we can pass it a Swagger JSON file and it will add that to its WebSockets API automatically, so we don't have to revert to using the REST API if we
are already connected with the WebSockets API. Fun!

It is also possible to use this one WebSocket API as a proxy for multiple REST APIs. So you could, for example, use this to retrieve data from multiple systems, aggregate
the data, and then push it out to all the other connected WebSocket clients. Handy!

## Configuration
A sample JSON configuration file has been provided, showing the key bits that are needed to get this up and running. Note that most of the options are required or the
agent will simply refuse to start. Parsing C in JSON isn't anyone's idea of fun, so this isn't particularly robust in that respect.
The JSON configuration file naturally needs to be customized for each system (the first Agent Server part) while at the same time, each such system in a particular
configuration would also be listed later on, in the Agent Systems part. While the agents don't (currently) communicate with one another directly, the Python client
(see below) makes it easy to connect and test agents. It uses the same configuration JSON to help make this a little easier.

## AgentP
Having a shiny new C-based WebSocket server is great and all, but not of much use out of the gate unless we can test it. 
To help with that, a Python script called AgentP has been provided. It can be pointed at the same JSON configuration file, and then 
selecting a system to connect to will give you the ability to send and receive messages (JSON messages, that is). 

## AgentU
And having a shiny new Python client is great and all, but maybe you're more interested in something with an actual UI? Well, here's AgentP to the rescue.
This is a rework of AgentP that uses Python and a UI library to give you the full UI experience. It isn't fancy, but maybe more friendly than the command line.

## Dependencies - C
While we're not doing anything particularly interesting here, we do need a few extra bits.
- curl -Used for sending SMTP e-mails and for the REST proxy 
- libwebsockets - The websockets interface, naturally
- jansson - A C library for handling JSON
- fswatch - To monitor OS-level file changes in a multiplatform manner
- xxhash - To generate file hashes, very quickly, so we can tell what has changed
- sqlite3 - For local storage of file data

## Dependencies - Python
A great deal of effort was spent trying to make a nice Python program. If there is such a thing.
In particular, playing nice with command-line history was a bit of work. With a bit of help.
- websockets - No surprise there
- Prompt Tookit - Made the seemingly impossible work pretty great
- AsyncIO - Wouldn't be here without it

## Remote Access
One of the main options when setting up an Agent is to select the port that it runs on. Currently, it will try and respond to all requests on that port. 
If you're only connecting to this agent locally, this is sufficient enough - just be sure to check that the port is not already in use. For remote access,
this can work just as well. However, typically we'd want to use a secured connection. The agent doesn't currently have any kind of SSL option built into it.

Instead, the connection to it can be proxied by Apache. So incoming connections from Apache can be forced through to SSL, and thus WSS: instead of WS: 
Apache can then proxy the connection to localhost://port and we don't even need to tell anyone what the port is, other than Apache, and no firewall access
is needed either, as the normal pathway through Apache is used. The main bit of interest is the proxy rules in Apache.

```
  ProxyPass "/agentc" "ws://localhost:23432"
  ProxyPassReverse "/agentc" "ws://localhost:23432"
```

## Example
Once an Agent has been configured, and its connection information added to the configuration JSON file, the AgentP program can be used to connect to it.
Here's what that might look like, connecting to a system named Festival. The commands sent are JSON, and the replies are JSON as well.

```
# ./agentp.py /usr/local/etc/Agent.json Festival
Establishing server connection to wss://www.srcdest.com/agentc
Server connection established
Enter JSON to send (Ctrl+C to quit): {"type":"status"}
Received JSON: {
  "status": "success",
  "launchTime": "2024-04-01 05:48:02",
  "agentServer": "Festival",
  "agentName": "AgentC",
  "agentVersion": "v1.0.0",
  "databaseVersion": "v1.0.0",
  "databaseFilename": "/usr/local/share/Agent.sqlite",
  "activeConnections": 1,
  "totalConnections": 1,
  "requests": 1,
  "memoryHighWatermark": 13596,
  "activeThreads": 2
}
Enter JSON to send (Ctrl+C to quit):
Agent exited
```
