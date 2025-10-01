# gRPC IRP 服务设置流程

基于 `irp.proto` 的 gRPC 服务实现，包含 Python 服务器和 C++ 客户端。

## 环境要求

- Python 3.13.5
- protoc 29.3
- macOS (Darwin 24.5.0)

## 完整流程命令记录

### 1. 验证 protobuf 工具链
```bash
# 检查 protoc 是否安装
which protoc
protoc --version

# 测试 proto 文件编译
protoc --python_out=. irp.proto
```

### 2. 安装 Python gRPC 依赖
```bash
pip3 install --break-system-packages grpcio grpcio-tools
```

### 3. 生成 gRPC Python 代码
```bash
python3 -m grpc_tools.protoc --proto_path=. --python_out=. --grpc_python_out=. irp.proto
```

### 4. 运行 Python 服务器
```bash
python3 server.py
```

### 5. 测试服务器（新终端）
```bash
python3 test_client.py
```

## 文件说明

- `irp.proto` - 原始 protobuf 定义文件
- `irp_pb2.py` - 生成的 protobuf 消息类
- `irp_pb2_grpc.py` - 生成的 gRPC 服务类
- `server.py` - Python gRPC 服务器实现
- `test_client.py` - Python 测试客户端

## 服务器功能

服务器实现了 `IrpService.ProcessOptimization` 方法，接收 `OuReq` 请求并返回 `OuResp` 响应。

当前实现为示例逻辑：
- 接收客户端的优化请求
- 处理每个场景信息
- 返回修改后的场景结果（增加成本、调整计划等）

## 下一步

- [ ] 实现 C++ 客户端
- [ ] 添加真实的优化算法
- [ ] 添加错误处理和验证
- [ ] 添加配置文件支持 