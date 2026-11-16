FROM ubuntu:24.04

RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential make \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# Builds ./netsim (the Makefile in the repo root)
RUN make

# The server must run from the project root so it can find frontend/
CMD ["sh", "-c", "./netsim ${PORT:-8080}"]
