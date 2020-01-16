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

lokirpc = None
x_key = None

badargs = False
if len(sys.argv) < 3:
    badargs = True
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
    print("\nUsage: {} localhost:22023 PUBKEY -- pings PUBKEY's quorumnet.\n\n"
            "If the given RPC is a SN (with unrestricted RPC), uses its x25519 key, otherwise generates a random one.")
    sys.exit(1)

missed = set(sys.argv[2:])
r = requests.post(lokirpc, json={"jsonrpc":"2.0", "id":0, "method":"get_service_nodes", "params": {
    "service_node_pubkeys": sys.argv[2:]}}).json()

context = zmq.Context()
states = r["result"]["service_node_states"] if "result" in r and "service_node_states" in r["result"] else []
tag = 1
for s in states:
    pk = s["service_node_pubkey"]
    if pk in missed:
        missed.remove(pk)
    ip, port = s["public_ip"], s["quorumnet_port"]
    if not ip or not port:
        print("SN {} has no IP/qnet port: {}:{}".format(pk, ip, port))
    socket = context.socket(zmq.DEALER)
    socket.curve_secretkey = x_key.encode()
    socket.curve_publickey = x_key.public_key.encode()
    socket.curve_serverkey = bytes.fromhex(s["pubkey_x25519"])
    socket.setsockopt(zmq.CONNECT_TIMEOUT, 5000)
    socket.setsockopt(zmq.HANDSHAKE_IVL, 5000)
    socket.setsockopt(zmq.IMMEDIATE, 1)
    socket.connect("tcp://{}:{}".format(ip, port))
    print("Ping {}:{} (for SN {})".format(ip, port, pk))
    bt_tag = bytes("i{}e".format(tag), "utf-8")
    socket.send_multipart((b"ping", b"d1:!" + bt_tag + b"e"))
    ponged = False
    while socket.poll(timeout=5000):
        m = socket.recv_multipart()
        if len(m) == 2 and m[0] == b'pong':
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
