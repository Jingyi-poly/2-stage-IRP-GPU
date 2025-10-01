which protoc 来检查proto的安装情况 /opt/homebrew/bin/protoc安装在这里？
protoc --version   ->  libprotoc 29.3

在irp.proto目录下运行
protoc --python_out=. irp.proto
如果生成irp_pb2.py 则说明python侧的协议生成没有问题

pip3 install --break-system-packages grpcio grpcio-tools

python3 -m grpc_tools.protoc --proto_path=. --python_out=. --grpc_python_out=. irp.proto
成功运行会生成irp_pb2_grpc.py

python3 -m grpc_tools.protoc --proto_path=. --python_out=. --grpc_python_out=. irp.proto
重新生成

python3 server.py    运行  + &


ps aux | grep "python3 server.py" | grep python


pkill -f "python3 server.py"
pkill -f "python server.py"


cpp

brew install grpc

protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=/opt/homebrew/bin/grpc_cpp_plugin irp.proto
会生成四个文件

protoc --version

brew reinstall protobuf

protoc --cpp_out=. --grpc_out=. --plugin=protoc-gen-grpc=/opt/homebrew/bin/grpc_cpp_plugin irp.proto

make clean && make

brew install pkg-config

pkg-config --list-all | grep -E 'grpc|protobuf|absl'

pkg-config --cflags grpc++ protobuf absl_strings absl_synchronization absl_time absl_cord

pkg-config --libs grpc++ protobuf absl_strings absl_synchronization absl_time absl_cord


make clean && make



pip3 install --upgrade protobuf

pip3 install --upgrade protobuf --break-system-packages

protoc --python_out=. irp.proto

ps aux | grep "python3 server.py" | grep -v grep

lsof -i :50051

kill 87902 96498