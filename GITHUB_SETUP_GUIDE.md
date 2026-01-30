# GitHub 推送完整指南

本指南将帮助您将 OUIRP-GPU 项目推送到 GitHub。

## 步骤 1: 配置 Git 用户信息

首先，配置您的 Git 用户信息（如果还没有配置）：

```bash
# 设置您的 GitHub 用户名
git config --global user.name "Your Name"

# 设置您的 GitHub 邮箱
git config --global user.email "your.email@example.com"

# 验证配置
git config --global user.name
git config --global user.email
```

## 步骤 2: 初始化 Git 仓库

在项目根目录下初始化 Git 仓库：

```bash
cd /Users/zhanghaohua/Codebase/OUIRP-GPU

# 初始化 Git 仓库
git init

# 验证初始化成功
git status
```

您应该看到类似 "On branch main" 或 "On branch master" 的输出。

## 步骤 3: 添加文件到 Git

将项目文件添加到 Git 暂存区：

```bash
# 添加所有文件（.gitignore 会自动排除不需要的文件）
git add .

# 查看将要提交的文件
git status
```

检查输出，确保没有包含不应该提交的文件（如 build/ 目录、.o 文件等）。

## 步骤 4: 创建初始提交

创建第一个提交：

```bash
# 创建提交
git commit -m "Initial commit: OUIRP-GPU solver with RPC support

- Hybrid Genetic Search algorithm implementation
- Two-stage stochastic optimization
- gRPC-based GPU acceleration support
- Complete documentation and examples"

# 验证提交成功
git log --oneline
```

## 步骤 5: 在 GitHub 创建远程仓库

### 方法 A: 通过 GitHub 网页创建（推荐）

1. 访问 https://github.com/new
2. 填写仓库信息：
   - **Repository name**: `OUIRP-GPU`
   - **Description**: `Hybrid Genetic Search for the Inventory Routing Problem with GPU Acceleration`
   - **Visibility**: 选择 Public 或 Private
   - **不要勾选** "Initialize this repository with a README"（因为我们已经有了）
   - **不要添加** .gitignore 或 license（我们已经有了）
3. 点击 "Create repository"
4. 记下仓库 URL（下一步需要用到）

### 方法 B: 使用 GitHub CLI（如果已安装）

```bash
# 安装 GitHub CLI（如果还没有）
# macOS: brew install gh
# 其他系统: https://cli.github.com/

# 登录 GitHub
gh auth login

# 创建仓库
gh repo create OUIRP-GPU --public --source=. --remote=origin --push
```

如果使用方法 B，可以跳过步骤 6。

## 步骤 6: 连接远程仓库并推送

### 选项 A: 使用 HTTPS（简单，但每次需要输入密码或 token）

```bash
# 添加远程仓库（替换 YOUR_USERNAME 为您的 GitHub 用户名）
git remote add origin https://github.com/YOUR_USERNAME/OUIRP-GPU.git

# 设置默认分支名称为 main（如果当前是 master）
git branch -M main

# 推送到 GitHub
git push -u origin main
```

**注意**: GitHub 现在要求使用 Personal Access Token 而不是密码。如果提示输入密码：
1. 访问 https://github.com/settings/tokens
2. 点击 "Generate new token (classic)"
3. 选择 `repo` 权限
4. 生成 token 并复制
5. 在推送时使用 token 作为密码

### 选项 B: 使用 SSH（推荐，一次配置，永久使用）

#### 6.1 检查是否已有 SSH 密钥

```bash
ls -la ~/.ssh
```

如果看到 `id_rsa.pub` 或 `id_ed25519.pub`，说明已有密钥，跳到 6.3。

#### 6.2 生成新的 SSH 密钥

```bash
# 生成 SSH 密钥（替换为您的 GitHub 邮箱）
ssh-keygen -t ed25519 -C "your.email@example.com"

# 按 Enter 使用默认文件位置
# 可以设置密码短语（推荐）或直接按 Enter

# 启动 ssh-agent
eval "$(ssh-agent -s)"

# 添加 SSH 密钥到 ssh-agent
ssh-add ~/.ssh/id_ed25519
```

#### 6.3 添加 SSH 密钥到 GitHub

```bash
# 复制公钥到剪贴板
cat ~/.ssh/id_ed25519.pub | pbcopy
# 或者手动复制输出内容
cat ~/.ssh/id_ed25519.pub
```

然后：
1. 访问 https://github.com/settings/keys
2. 点击 "New SSH key"
3. Title: 填写 "MacBook Pro" 或其他描述
4. Key: 粘贴刚才复制的公钥
5. 点击 "Add SSH key"

#### 6.4 使用 SSH 推送

```bash
# 添加远程仓库（替换 YOUR_USERNAME）
git remote add origin git@github.com:YOUR_USERNAME/OUIRP-GPU.git

# 测试 SSH 连接
ssh -T git@github.com
# 应该看到: "Hi YOUR_USERNAME! You've successfully authenticated..."

# 设置默认分支名称为 main
git branch -M main

# 推送到 GitHub
git push -u origin main
```

## 步骤 7: 验证推送成功

访问您的 GitHub 仓库页面：
```
https://github.com/YOUR_USERNAME/OUIRP-GPU
```

您应该能看到：
- ✅ 所有项目文件
- ✅ README.md 正确显示
- ✅ 文件结构完整
- ✅ 提交历史

## 后续操作

### 日常工作流程

```bash
# 1. 修改文件后，查看更改
git status
git diff

# 2. 添加更改
git add .
# 或添加特定文件
git add path/to/file

# 3. 提交更改
git commit -m "描述您的更改"

# 4. 推送到 GitHub
git push
```

### 创建分支进行开发

```bash
# 创建并切换到新分支
git checkout -b feature/new-feature

# 进行开发和提交
git add .
git commit -m "Add new feature"

# 推送分支到 GitHub
git push -u origin feature/new-feature

# 在 GitHub 上创建 Pull Request
```

### 更新 README 中的仓库 URL

推送成功后，记得更新 README.md 中的占位符：

```bash
# 编辑 README.md，将 <repository-url> 替换为实际 URL
# 例如: https://github.com/YOUR_USERNAME/OUIRP-GPU.git

git add README.md
git commit -m "Update repository URL in README"
git push
```

## 常见问题

### Q1: 推送时提示 "Permission denied"
**解决方案**:
- 检查 SSH 密钥是否正确添加到 GitHub
- 运行 `ssh -T git@github.com` 测试连接
- 确保使用正确的远程 URL

### Q2: 推送时提示 "Updates were rejected"
**解决方案**:
```bash
# 先拉取远程更改
git pull origin main --rebase

# 然后再推送
git push
```

### Q3: 想要修改最后一次提交信息
**解决方案**:
```bash
# 修改最后一次提交
git commit --amend -m "新的提交信息"

# 强制推送（仅在还没有其他人拉取时使用）
git push --force
```

### Q4: 不小心提交了敏感信息
**解决方案**:
```bash
# 从历史中移除文件
git filter-branch --force --index-filter \
  "git rm --cached --ignore-unmatch path/to/sensitive/file" \
  --prune-empty --tag-name-filter cat -- --all

# 强制推送
git push --force --all
```

## 推荐的 GitHub 仓库设置

推送成功后，在 GitHub 仓库页面进行以下设置：

1. **添加 Topics**:
   - 点击仓库页面的 "Add topics"
   - 添加: `inventory-routing`, `genetic-algorithm`, `optimization`, `gpu-acceleration`, `grpc`, `cpp17`, `pytorch`

2. **设置 About**:
   - Description: `Hybrid Genetic Search for the Inventory Routing Problem with GPU Acceleration`
   - Website: 如果有相关网站
   - 勾选 "Releases" 和 "Packages"

3. **启用 Issues**:
   - Settings → Features → 勾选 "Issues"

4. **添加 Branch Protection**（可选）:
   - Settings → Branches → Add rule
   - Branch name pattern: `main`
   - 勾选 "Require pull request reviews before merging"

## 完成！

恭喜！您的项目现在已经在 GitHub 上了。您可以：
- 📝 继续开发并推送更新
- 🌟 邀请其他人 star 您的项目
- 🤝 接受其他开发者的贡献
- 📊 使用 GitHub Actions 进行 CI/CD
- 📦 发布 Releases

如有任何问题，请参考 [GitHub 官方文档](https://docs.github.com/)。
