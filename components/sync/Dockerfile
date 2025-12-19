FROM ubuntu:20.04
ENV DEBIAN_FRONTEND=noninteractive
WORKDIR /workspace

# Proxy (optional; can be overridden at build time via --build-arg)
ARG http_proxy="http://172.31.225.139:7890"
ARG https_proxy="http://172.31.225.139:7890"
ENV http_proxy=${http_proxy} \
    https_proxy=${https_proxy} \
    HTTP_PROXY=${http_proxy} \
    HTTPS_PROXY=${https_proxy}

# Dev deps only. MongoDB C++ driver + RocketMQ C++ client are intentionally NOT built here.
# This base uses Ubuntu 20.04 (Boost 1.71.x, <1.72) to match older RocketMQ C++ client code.
RUN apt-get update && apt-get install -y --no-install-recommends \
    bash \
    ca-certificates \
    curl \
    cmake \
    g++ \
    git \
    make \
    pkg-config \
    python3 \
    libssl-dev \
    libsasl2-dev \
    libcurl4-openssl-dev \
    zlib1g-dev \
    libevent-dev \
    libjsoncpp-dev \
    libboost-all-dev \
    && rm -rf /var/lib/apt/lists/*
