# Live network configuration

The package does not provision a relay. Set up a managed TURN service or a TURN
server that supports time-limited TURN REST credentials using an HMAC-SHA1
shared secret. The existing Hostinger PHP host is the signaling/control service;
it is not the TURN server.

Add these keys to the returned array in the EXISTING private
`lindell-private/config.php`. Preserve every existing configuration entry.
These are placeholders, not working credentials:

```php
'live_stun_urls' => ['stun:YOUR-RELAY-HOST:3478'],
'live_turn_urls' => [
    'turn:YOUR-RELAY-HOST:3478?transport=udp',
    'turn:YOUR-RELAY-HOST:3478?transport=tcp',
    'turns:YOUR-RELAY-HOST:5349?transport=tcp',
],
'live_turn_secret' => 'REPLACE-WITH-YOUR-TURN-REST-SHARED-SECRET',
```

The server must be configured for the SAME shared secret and have the appropriate
UDP/TCP/TLS ports, relay port range and valid TLS certificate. Merely pasting these
placeholders into config.php will not establish a relay. The secret must be at
least 24 characters and stays in the private config. Ensure server clocks agree.

The website issues random-suffixed timestamp usernames and base64 HMAC-SHA1
credentials, valid for 600 seconds. Static-credential-only providers need a
separate adapter; do not paste static account passwords into these fields.
No public STUN/TURN provider is selected or contacted by default.

Acceptance: connect actual plugin and listener on different internet connections,
including a mobile network and restrictive Wi-Fi; test UDP blocked, TURN/TCP and
TURN/TLS, credential expiry and long sessions. Confirm stereo audio and CPU/upload
load with your intended number of listeners. Local-host success proves the
signaling/player path, not real-world traversal or native AAX behavior.
