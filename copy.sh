containerId="2697757d708a"
code_file="./ast-interpreter/"
testcases="./testcase"

tempBuildFolder="/root/build/"
container_code_file="/root/${code_file}/"
echo "copy code..."
docker exec ${containerId} rm -r ${container_code_file}
docker cp ${code_file} ${containerId}:/root/
docker cp ${testcases} ${containerId}:/root/
