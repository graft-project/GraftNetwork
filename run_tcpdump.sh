# Make sure that eth0 interface exists and supernode is connecting to 28900 port.
nohup sudo tcpdump -i eth0 'port 28900' -v &> ./logs/tcpdump28900.log &
