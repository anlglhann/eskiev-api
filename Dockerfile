FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ cmake make && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN cmake -S . -B build && cmake --build build -j

EXPOSE 8080
CMD ["./build/eskiev_api"]