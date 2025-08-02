# syntax=docker/dockerfile:1

FROM ubuntu:noble

RUN apt-get update \
 && apt-get install -y \
      ca-certificates \
      gnupg2 \
      software-properties-common \
      wget \
 && wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key \
  | gpg --dearmor - \
  > /usr/share/keyrings/llvm-archive-keyring.gpg \
 && echo 'deb [signed-by=/usr/share/keyrings/llvm-archive-keyring.gpg] https://apt.llvm.org/noble/ llvm-toolchain-noble-19 main' > /etc/apt/sources.list.d/llvm.list \
 && wget -O - http://packages.lunarg.com/lunarg-signing-key-pub.asc \
  | gpg --dearmor - \
  > /usr/share/keyrings/lunarg-archive-keyring.gpg \
 && echo 'deb [signed-by=/usr/share/keyrings/lunarg-archive-keyring.gpg] https://packages.lunarg.com/vulkan/ noble main' > /etc/apt/sources.list.d/lunarg.list \
 && wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc \
  | gpg --dearmor - \
  > /usr/share/keyrings/kitware-archive-keyring.gpg \
 && echo 'deb [signed-by=/usr/share/keyrings/kitware-archive-keyring.gpg] https://apt.kitware.com/ubuntu/ noble main' > /etc/apt/sources.list.d/kitware.list \
 && apt-add-repository ppa:ubuntu-toolchain-r/ppa \
 && apt-get update \
 && DEBIAN_FRONTEND=noninteractive \
      apt-get install -y \
      clang-19 \
      clang-format-19 \
      clang-tidy-19 \
      clang-tools-19 \
      cmake \
      g++-13 \
      gcc-13 \
      gcovr \
      git \
      libclang-19-dev \
      libfreetype-dev \
      libharfbuzz-dev \
      libllvmlibc-19-dev \
      libstdc++-13-dev \
      liburing-dev \
      libwayland-dev \
      libx11-dev \
      libxcb-randr0-dev \
      libxcb-xkb-dev \
      libxcb-xinput-dev \
      libxkbcommon-dev \
      libxkbcommon-x11-dev \
      libxxhash-dev \
      libzstd-dev \
      llvm-19-dev \
      ninja-build \
      parallel \
      shaderc \
      wayland-protocols \
      wayland-utils \
 && rm -rf \
   /var/lib/apt/lists/* \
 && ln -sf /usr/bin/g++-13 /usr/bin/g++ \
 && ln -sf /usr/bin/gcov-13 /usr/bin/gcov \
 && git clone https://github.com/include-what-you-use/include-what-you-use.git \
 && git -C include-what-you-use checkout dc9fd2c258a9f29881d73330c9b6fdb13b41442c \
 && cmake \
   -Biwyu-build \
   -GNinja \
   -DCMAKE_BUILD_TYPE=Release \
   -DCMAKE_INSTALL_PREFIX=/usr \
   ./include-what-you-use \
 && ninja -Ciwyu-build install \
 && rm /usr/lib/clang/19/include \
 && cp -r iwyu-build/lib/clang/19/include/ /usr/lib/clang/19/include/ \
 && rm -r include-what-you-use iwyu-build \
 && ln -sf /usr/bin/clang-19 /usr/bin/clang \
 && ln -sf /usr/bin/clang++-19 /usr/bin/clang++ \
 && ln -sf /usr/bin/clang-format-19 /usr/bin/clang-format \
 && ln -sf /usr/bin/clang-tidy-19 /usr/bin/clang-tidy

ENV LD_LIBRARY_PATH=/usr/lib/llvm-19/lib