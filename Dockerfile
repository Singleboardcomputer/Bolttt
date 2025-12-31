# syntax=docker/dockerfile:1.7

FROM --platform=$TARGETPLATFORM rust:1.92-bookworm AS builder

WORKDIR /build

# system deps Pumpkin actually needs
RUN apt-get update && apt-get install -y \
    pkg-config \
    libssl-dev \
 && rm -rf /var/lib/apt/lists/*

# clone Pumpkin INSIDE the container
RUN git clone https://github.com/Pumpkin-MC/Pumpkin.git
WORKDIR /build/Pumpkin

# build (native ARM64 under QEMU)
RUN cargo build -p pumpkin --release

# --------------------------------------------------

FROM debian:bookworm-slim

RUN apt-get update && apt-get install -y \
    ca-certificates \
    libssl3 \
 && rm -rf /var/lib/apt/lists/*

COPY --from=builder /build/Pumpkin/target/release/pumpkin /usr/local/bin/pumpkin

EXPOSE 25565
ENTRYPOINT ["pumpkin"]
