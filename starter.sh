rm -rf build
mkdir build
cd build
mkdir generated
protoc \
  --proto_path=../proto \
  --cpp_out=generated \
  --grpc_out=generated \
  --plugin=protoc-gen-grpc=$(which grpc_cpp_plugin) \
  ../proto/common.proto \
  ../proto/file_audit.proto \
  ../proto/block_chain.proto

