FROM debian:bookworm

RUN apt-get update || true && \
    apt-get update && \
    apt-get install -y --fix-missing \
    gcc-arm-linux-gnueabihf \
    python3 \
    python3-pip \
    git \
    make \
    nano \
    android-tools-adb \
    android-tools-fastboot && \
    apt-get clean && \
    rm -rf /var/lib/apt/lists/*

RUN ln -s /usr/bin/python3 /usr/bin/python
COPY utils/requirements.txt /tmp/requirements.txt
RUN pip install --break-system-packages -r /tmp/requirements.txt