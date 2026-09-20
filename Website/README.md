# Listener diagnostics 0.4.0

Optional companion to the 0.4.0 VST3. Back up the deployed public_html/live.js, then replace it with this file and reload the page (a private browsing tab avoids an old cached script).

Adds Show connection diagnostics to the Live dialog. It reports server counts, ICE gathering state, candidate counts, whether the offer was registered and whether the answer was received. It does not include SDP, addresses, playlist links, private keys or temporary relay passwords.

This file is based on the recovered Live website package. It does not change the PHP Cloudflare adapter, playlist auth or audio routing. The deployed live.php must already read lindell-private/cloudflare-turn.json.

Mac VST3 relay testing: enable Test TLS relay before Connect, then Go Live. Open the shared playlist on the phone with Wi-Fi off and press Listen Live. Copy diagnostics from the plugin and use Show connection diagnostics on the phone if it fails. A TLS-verified socket is not proof of a TURN allocation; check relay candidate count and selected route. Only connected listeners trigger the ON AIR indicator.
