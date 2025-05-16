mkdir -p keys
# 1) Generate the private key
openssl genpkey -algorithm RSA \
                -out client_private.pem \
                -pkeyopt rsa_keygen_bits:2048

# 2) Extract the public key
openssl rsa -in client_private.pem \
            -pubout \
            -out client_public.pem
mkdir -p blocks
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

