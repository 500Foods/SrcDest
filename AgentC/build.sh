#!/bin/bash
#gcc -g agentc.c -o agentc -lsqlite3 -lxxhash -ljansson -lwebsockets -lfswatch -lcurl -lssl -lcrypto -I/usr/include/libfswatch/c -fsanitize=address
gcc    agentc.c -o agentc -lsqlite3 -lxxhash -ljansson -lwebsockets -lfswatch -lcurl -lssl -lcrypto -I/usr/include/libfswatch/c

