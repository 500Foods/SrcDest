#!/bin/bash

# For troubleshooting memory issues
# gcc -g agentc.c -o agentc -lsqlite3 -lxxhash -ljansson -lwebsockets -lfswatch -I/usr/include/libfswatch/c -fsanitize=address

# Normally agentc would land in /usr/local/bin/agentc for example
gcc agentc.c -o agentc -lsqlite3 -lxxhash -ljansson -lwebsockets -lfswatch -I/usr/include/libfswatch/c
