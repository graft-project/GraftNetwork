#!/usr/bin/python3

import nacl.bindings as sodium
from nacl.public import PrivateKey
from nacl.signing import SigningKey, VerifyKey
import nacl.encoding
import requests
import zmq
import sys
import re
from os import path

def hexstr(key):
    return key.encode(encoder=nacl.encoding.HexEncoder)  #.decode('utf-8')

direct = None
lokirpc = None
x_key = None

badargs = False
if len(sys.argv) < 3:
    badargs = True
elif len(sys.argv) == 3 and re.match(r"[0-9a-fA-F]{64}", sys.argv[1]) and re.match(r"(?:tcp|ipc)://.*", sys.argv[2]):
    direct = (sys.argv[1], sys.argv[2])
    x_key = PrivateKey.generate()
else:
    m = re.match(r"((?:[a-zA-Z0-9-]+\.)*[a-zA-Z0-9-]+):(\d{4,5})", sys.argv[1])
    if m:
        lokirpc = "http://{}:{}/json_rpc".format(m.group(1), m.group(2))
        r = requests.post(lokirpc, json={"jsonrpc":"2.0", "id":0, "method":"get_service_node_privkey"}).json()
        if "result" in r and "service_node_x25519_privkey" in r["result"]:
            x_key = PrivateKey(r["result"]["service_node_x25519_privkey"], encoder=nacl.encoding.HexEncoder)
            print("Loaded x25519 keys (pub: {}) from lokid @ {}".format(hexstr(x_key.public_key), sys.argv[1]))
        else:
            x_key = PrivateKey.generate()
            print("Generated x25519 key {} (lokid @ {} did not return SN privkeys)".format(hexstr(x_key.public_key), sys.argv[1]))
    else:
        print("Error: {} does not look like a valid lokid RPC host:port value".format(sys.argv[1]), sys.stderr)
        badargs = True

    for pk in sys.argv[2:]:
        if len(pk) != 64 or not all(x in "abcdef0123456789" for x in pk):
            print("Error: {} is not a SN pubkey".format(pk), file=sys.stderr)
            badargs = True

if badargs:
    print("\nUsage: {0} localhost:22023 PUBKEY -- pings PUBKEY's quorumnet, looking up the address via the given RPC address.\n"
            "       {0} XPUBKEY tcp://1.2.3.4:5678 -- ping some SN at the given address.\n\n"
            "If the given RPC is a SN (with unrestricted RPC), uses its x25519 key, otherwise generates a random one."
            .format(sys.argv[0])
            )
    sys.exit(1)

if direct:
    missed = set()
    states = [{"service_node_pubkey": direct[0], "pubkey_x25519": direct[0], "_connect": direct[1]}]
else:
    missed = set(sys.argv[2:])
    r = requests.post(lokirpc, json={"jsonrpc":"2.0", "id":0, "method":"get_service_nodes", "params": {
        "service_node_pubkeys": sys.argv[2:]}}).json()
    states = r["result"]["service_node_states"] if "result" in r and "service_node_states" in r["result"] else []


context = zmq.Context()
tag = 1
for s in states:
    pk = s["service_node_pubkey"]
    if pk in missed:
        missed.remove(pk)
    if "_connect" not in s:
        ip, port = s["public_ip"], s["quorumnet_port"]
        if not ip or not port:
            print("SN {} has no IP/qnet port: {}:{}".format(pk, ip, port))
    else:
        ip, port = None, None
    socket = context.socket(zmq.DEALER)
    socket.curve_secretkey = x_key.encode()
    socket.curve_publickey = x_key.public_key.encode()
    socket.curve_serverkey = bytes.fromhex(s["pubkey_x25519"])
    socket.setsockopt(zmq.CONNECT_TIMEOUT, 5000)
    socket.setsockopt(zmq.HANDSHAKE_IVL, 5000)
    socket.setsockopt(zmq.IMMEDIATE, 1)
    if "_connect" in s:
        socket.connect(s["_connect"])
        print("Ping {} (for SN {})".format(s["_connect"], pk))
    else:
        socket.connect("tcp://{}:{}".format(ip, port))
        print("Ping {}:{} (for SN {})".format(ip, port, pk))

    bt_tag = bytes("i{}e".format(tag), "utf-8")
    socket.send_multipart((b"ping.ping", b"d1:!" + bt_tag + b"e"))
    ponged = False
    while socket.poll(timeout=5000):
        m = socket.recv_multipart()
        if len(m) == 2 and m[0] == b'ping.pong':
            ponged = True
            if m[1] == b'd1:!' + bt_tag + b'2:sni1ee':
                print("Received pong, we were recognized as a SN")
            elif m[1] == b'd1:!' + bt_tag + b'2:sni0ee':
                print("Received pong, we were recognized as non-SN")
            else:
                print("Received unexpected pong reply: {}".format(m[1]))
            break
        print("Received unexpected message:".format(len(m)))
        for i in m:
            print(" - {}".format(i))

    if not ponged:
        print("TIMEOUT!");
    tag += 1
