FROM ubuntu:18.04

WORKDIR /workspace
RUN apt update && apt install -y \
  build-essential \
  wget \
  gawk \
  gdb \
  flex \
  m4 \
  autoconf \
  texinfo  \
  vim \
  git \
  ninja-build \
  zlib1g-dev \
  python3-dev \
  make \
  cmake \
  python-dev \
  bison 

COPY . /workspace

RUN mkdir -p /workspace/build && cd /workspace/build && ../configure.sh 
RUN cd /workspace/build/ && cmake --build . --target install 2>&1 | tee build.log

CMD [ "/bin/bash" ]
