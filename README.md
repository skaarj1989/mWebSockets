# Testing μWebSockets implementation

## Prerequisites

1. [Docker](https://www.docker.com/)
2. [crossbario/autobahn-testsuite](https://github.com/crossbario/autobahn-testsuite)

> The `autobahn-testsuite` image will be downloaded automatically on the first time use of `fuzzing(client/server).bat`

## Client

1. Adjust settings in `config/fuzzingserver.json`
   - Most likely you'll have to change the `url` only
2. Run `fuzzingserver.bat`
3. Upload `test/fuzzingclient.ino` to a board
4. Check the resulting report in `reports/clients/index.html`

## Server

1. Adjust settings in `config/fuzzingclient.json`
2. Upload `test/fuzzingserver.ino` to a board
3. Run `fuzzingclient.bat`
4. Check the resulting report in `reports/servers/index.html`
