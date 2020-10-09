containerId="2697757d708a"
code_file="./sample/" #change to code files later
testcases="./testcase"

tempBuildFolder="/root/build/"
container_code_file="/root/ast-interpreter"
echo "copy code..."
docker exec ${containerId} rm -r ${container_code_file}
docker cp ${code_file} ${containerId}:/root/
docker cp ${testcases} ${containerId}:/root/
echo "buiding..."
docker exec ${containerId} rm -r ${tempBuildFolder}
docker exec ${containerId} mkdir ${tempBuildFolder}
docker exec -w ${tempBuildFolder} ${containerId}  cmake -DLLVM_DIR=/usr/local/llvm10ra/ ${container_code_file}
docker exec -w ${tempBuildFolder} ${containerId} make
echo "running container..."
docker exec -it ${containerId} /bin/bash
