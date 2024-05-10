FROM ubuntu:latest

RUN apt-get update \
 && apt-get install -y \
        ca-certificates \
        catch2 \
        cmake \
        g++ \
        libcli11-dev \
        libhdf5-openmpi-dev \
        librange-v3-dev \
        lsb-release \
        ninja-build \
        nlohmann-json3-dev \
        wget
RUN wget https://apache.jfrog.io/artifactory/arrow/$(lsb_release --id --short | tr 'A-Z' 'a-z')/apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb \
 && apt-get install -y ./apache-arrow-apt-source-latest-$(lsb_release --codename --short).deb \
 && apt-get update \
 && apt-get install -y \
        libarrow-dev \
        libparquet-dev

ENV OMPI_ALLOW_RUN_AS_ROOT=1
ENV OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

RUN cmake -GNinja -B /build -S /workspace -DCMAKE_CXX_COMPILER=/usr/bin/mpicxx \
 && cmake --build /build \
 && cmake --install /build \
 && cd /build \
 && ctest --output-on-failure
