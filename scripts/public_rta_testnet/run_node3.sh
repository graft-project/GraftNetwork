DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

${DIR}/../../build/release/bin/graftnoded --testnet --detach

nohup ${DIR}/../../build/release/bin/graft-supernode ${DIR}/conf_node3.ini > /dev/null 2>&1 &

