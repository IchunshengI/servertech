# 基础镜像：官方 Python 3.12
FROM python:3.12

# 时区 & pip 相关环境变量
ENV TZ=Asia/Shanghai \
    PIP_DEFAULT_TIMEOUT=100

# 如果想直接在镜像里固定清华源，可以打开下面这一行（可选）
ENV PIP_INDEX_URL=https://pypi.tuna.tsinghua.edu.cn/simple

# 安装必要工具
RUN apt-get update && apt-get install -y \
    curl \
    ca-certificates \
    xz-utils \
    gnupg \
    net-tools \
    iproute2 \
    && rm -rf /var/lib/apt/lists/*

# 安装 Node.js (>= 22.10)
ARG NODE_VERSION=22.12.0

RUN set -eux; \
    arch="$(dpkg --print-architecture)"; \
    case "$arch" in \
      amd64) nodeArch='x64' ;; \
      arm64) nodeArch='arm64' ;; \
      *) echo "Unsupported architecture: $arch" && exit 1 ;; \
    esac; \
    curl -fsSLO "https://nodejs.org/dist/v${NODE_VERSION}/node-v${NODE_VERSION}-linux-${nodeArch}.tar.xz"; \
    tar -xJf "node-v${NODE_VERSION}-linux-${nodeArch}.tar.xz" -C /usr/local --strip-components=1; \
    rm "node-v${NODE_VERSION}-linux-${nodeArch}.tar.xz"; \
    ln -s /usr/local/bin/node /usr/local/bin/nodejs; \
    node -v; \
    npm -v

# 工作目录
WORKDIR /workspace
