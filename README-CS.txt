### Client/Server Information

#### Running a server

Running a server is simple:

    `eternity -csserve [ configuration_file ]`

All configuration is done in the configuration file located at
`base/server.json`.  The location is configurable using the `-server-config`
command-line option.  See `docs/cs_server.config.txt` for more information on
the server configuration file.

Eternity server are normally headless; to spawn the server window use
`-show-server-window`.  Note that closing this window shuts the server down.

#### Running a client

Running the client is also simple:

    `eternity -csjoin [ URL ]`

`URL` points to a Eternity server URL.

It is recommended that Windows use the `-directx` command-line switch, and
Vista/7 users should also use `-8in32` to avoid color palette issues.
Additionally the volume of sound effects and music cannot be set independently
in Vista/7, so the use of `-nomusic` is encouraged for competitive play.

vim:tw=79 sw=4 ts=4 syntax=mkd et:

