#!/bin/bash

# opens gnome-terminal with 3 tabs, each node running in one tab

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
gnome-terminal --tab -e $DIR/start_node1.sh --tab -e $DIR/start_node2.sh  --tab -e $DIR/start_node3.sh 


