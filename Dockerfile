FROM debian:bookworm

RUN apt-get update && apt-get install -y \
    gcc-arm-linux-gnueabihf \
    python3 \
    python3-pip \
    git \
    make && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

COPY utils/requirements.txt /tmp/requirements.txt
RUN pip install --break-system-packages -r /tmp/requirements.txt