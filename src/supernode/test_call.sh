
#curl -X POST http://127.0.0.1:7655/json_rpc -d '{"jsonrpc":"2.0","id":"0","method":"test_call","params":""}' -H 'Content-Type: application/json'
curl -X POST http://127.0.0.1:7655/json_rpc -d '{"method":"test_call","params":""}' -H 'Content-Type: application/json' 


