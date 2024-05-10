FROM ubuntu:latest

RUN apt-get update \
 && apt-get install -y \
        ca-certificates \
        catch2 \
        cmake \
        g++ \
        git \
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

RUN mkdir /highfive \
 && git clone https://github.com/BlueBrain/HighFive /highfive/src \
 && cmake -B /highfive/build -S /highfive/src -DCMAKE_INSTALL_PREFIX=/highfive/install -DHIGHFIVE_UNIT_TESTS=OFF -DHIGHFIVE_EXAMPLES=OFF -DHIGHFIVE_BUILD_DOCS=OFF \
 && cmake --build /highfive/build \
 && cmake --install /highfive/build

ENV CMAKE_PREFIX_PATH=/highfive/install
ENV OMPI_ALLOW_RUN_AS_ROOT=1
ENV OMPI_ALLOW_RUN_AS_ROOT_CONFIRM=1

COPY . /src

RUN cmake -GNinja -B /build -S /src -DCMAKE_CXX_COMPILER=/usr/bin/mpicxx \
 && cmake --build /build \
 && cmake --install /build \
 && cd /build \
 && ctest --output-on-failure
