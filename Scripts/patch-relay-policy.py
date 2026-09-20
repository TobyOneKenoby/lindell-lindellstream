"""Apply narrow, idempotent relay-only policy support to pinned RTC dependencies."""
from pathlib import Path
import sys

root = Path(sys.argv[1])
def patch(path, before, after):
    file = root / path
    text = file.read_text()
    if after in text:
        return
    if text.count(before) != 1:
        raise RuntimeError(f"Unexpected dependency source: {path}")
    file.write_text(text.replace(before, after))

patch("deps/libjuice/include/juice/juice.h",
      "typedef struct juice_config {",
      "typedef struct juice_config {\n\tint lindell_relay_only;")
patch("src/impl/icetransport.cpp",
      "juice_config_t jconfig = {};",
      "juice_config_t jconfig = {};\n\tjconfig.lindell_relay_only = config.iceTransportPolicy == TransportPolicy::Relay;")
patch("deps/libjuice/src/agent.c",
      "ice_candidate_t *remote) {\n\tice_candidate_pair_t pair;",
      "ice_candidate_t *remote) {\n\tif (agent->config.lindell_relay_only && (!local || local->type != ICE_CANDIDATE_TYPE_RELAYED))\n\t\treturn 0;\n\tice_candidate_pair_t pair;")
