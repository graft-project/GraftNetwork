#!/usr/bin/python

import requests
import json
import argparse

def get_blockheader_by_height(url, block_height):
    # bitmonerod is running on the localhost and port of 18081
    

    # standard json header
    headers = {'content-type': 'application/json'}

    # the block to get
    

    # bitmonerod' procedure/method to call
    rpc_input = {
        "method": "getblockheaderbyheight",
        "params": {"height": block_height }     
    }

    # add standard rpc values
    rpc_input.update({"jsonrpc": "2.0", "id": "0"})

    # execute the rpc request
    response = requests.post(
        url,
        data=json.dumps(rpc_input),
        headers=headers)

    # pretty print json output
    result = response.json();
    return result


def main():

    parser = argparse.ArgumentParser(description='Get block hashes')


    parser.add_argument("--start_block", type=int, default=0, required=False,
                    help="starting block height")
    parser.add_argument("--end_block", type=int, required=False,
                    help="ending block height")
    parser.add_argument("--daemon_address", type=str, required=False,
                    default="http://localhost:18981",
                    help="ending block height")

    args = parser.parse_args()


    start_block = args.start_block
    end_block    = start_block
    
    if args.end_block is not None:
        end_block = args.end_block

    end_block = end_block + 1

    daemon_address = args.daemon_address

    url = daemon_address + "/json_rpc"
    for block in range(start_block, end_block):
        result = get_blockheader_by_height(url, block)["result"]
        
        if result["status"] == "OK":
            block_header = result["block_header"]
            # each row is:
            # block, timestamp, difficulty
            print("%u, %u, %u" % (block_header["height"], block_header["timestamp"], block_header["difficulty"]))

    # print(json.dumps(block_header, indent=4))
if __name__ == "__main__":
    main()
