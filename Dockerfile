FROM debian:bookworm-slim

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        bash \
        build-essential \
        ca-certificates \
        clang \
        clang-format \
        cmake \
        coreutils \
        curl \
        gdb \
        git \
        libopenmpi-dev \
        libpng-dev \
        make \
        ninja-build \
        openmpi-bin \
        perl \
        python3 \
    && rm -rf /var/lib/apt/lists/*

# OpenMPI refuse de s'exécuter en root et de démarrer plus de rangs que de coeurs sans
# --oversubscribe ; ces réglages rendent `mpirun` utilisable tel quel dans le conteneur.
ENV OMPI_MCA_rmaps_base_oversubscribe=1 \
    OMPI_MCA_btl_vader_single_copy_mechanism=none

WORKDIR /workspace
