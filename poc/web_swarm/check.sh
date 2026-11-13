#!/usr/bin/env bash
#
# End to end check: does a native worker register with the orchestrator over a
# real WebRTC data channel?
#
# This is not a nix checkPhase, and that is not an oversight. The nix build
# sandbox runs in a network namespace holding nothing but a loopback interface,
# and ICE finds no usable candidate there -- libdatachannel reports "Failed to
# gather local ICE candidates" and negotiation never completes. The binaries are
# fine; there is simply nothing to gather. So the check lives here and is run
# outside the sandbox:
#
#   nix-build -A poc.web_swarm.native
#   ./result/bin/kel_lbm_web_swarm_check
#
# Exits 0 on success, non-zero with both logs printed otherwise.

set -u

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
orchestrator="$here/kel_lbm_orchestrator"
worker="$here/kel_lbm_worker_native"

for binary in "$orchestrator" "$worker"; do
	if [ ! -x "$binary" ]; then
		echo "missing executable: $binary" >&2
		exit 1
	fi
done

workdir="$(mktemp -d)"
trap 'rm -rf "$workdir"' EXIT

orch_log="$workdir/orch.log"
worker_log="$workdir/worker.log"

# Port 0 asks the OS for a free one, so the port is read back rather than
# assumed -- otherwise two runs on one machine collide.
# Fast heartbeat so three beats land well inside the worker timeout.
"$orchestrator" --port 0 --heartbeat 300 --monitor 0 > "$orch_log" 2>&1 &
orch_pid=$!
trap 'kill $orch_pid 2>/dev/null; rm -rf "$workdir"' EXIT

port=""
for _ in $(seq 1 100); do
	port="$(sed -n 's|.*ws://0\.0\.0\.0:\([0-9]\+\).*|\1|p' "$orch_log")"
	[ -n "$port" ] && break
	if ! kill -0 $orch_pid 2>/dev/null; then
		echo "orchestrator exited before reporting a port:" >&2
		cat "$orch_log" >&2
		exit 1
	fi
	sleep 0.1
done

if [ -z "$port" ]; then
	echo "orchestrator never reported a port:" >&2
	cat "$orch_log" >&2
	exit 1
fi

echo "orchestrator listening on $port"

"$worker" \
	--url "ws://127.0.0.1:$port/" \
	--id check-native \
	--once \
	--pongs 3 \
	--timeout 20000 > "$worker_log" 2>&1
worker_rc=$?

kill $orch_pid 2>/dev/null
wait $orch_pid 2>/dev/null

fail(){
	echo "FAIL: $1" >&2
	echo "--- orchestrator ---" >&2; cat "$orch_log" >&2
	echo "--- worker ---" >&2;       cat "$worker_log" >&2
	exit 1
}

[ $worker_rc -eq 0 ] || fail "worker exited $worker_rc"

# Both sides have to agree the channel came up, not just the worker.
grep -q "channel open" "$orch_log" \
	|| fail "orchestrator never saw the control channel open"
grep -q "control channel open" "$worker_log" \
	|| fail "worker never saw the control channel open"

# The hello has to have been accepted, as a native worker, and answered.
grep -q "registered check-native (native)" "$orch_log" \
	|| fail "orchestrator never registered the worker as native"
grep -q "<- welcome" "$worker_log" \
	|| fail "worker never received its welcome"

# Liveness, not just registration: the heartbeat has to be running and the
# worker has to be answering it.
grep -q "answered 3 ping(s)" "$worker_log" \
	|| fail "worker did not answer three heartbeats"

echo "PASS: native worker registered over a real data channel"
