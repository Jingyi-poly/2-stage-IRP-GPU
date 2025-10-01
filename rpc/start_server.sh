#!/bin/bash

echo "启动gRPC服务器..."
echo "按Ctrl+C停止服务器"

# 检查Python依赖
echo "检查Python依赖..."
python3 -c "import grpc, irp_pb2, irp_pb2_grpc" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "错误: 缺少Python依赖。请安装:"
    echo "pip3 install grpcio grpcio-tools protobuf"
    exit 1
fi

# 启动服务器
echo "启动服务器在端口50051..."
python3 server.py 