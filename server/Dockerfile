# Use an official Debian image as the base image
FROM debian:bullseye-slim

# Install any necessary dependencies (e.g., gcc, make, libc6-dev)
RUN apt-get update && apt-get install -y gcc make libc6-dev libhiredis-dev libevent-dev

# Create a directory in the container to hold your application's source code
WORKDIR /app

# Copy your C application's source code and Makefile to the container
COPY . .

# Build your C application using the Makefile
RUN make clean
RUN make

EXPOSE 8080

# Define the command to run when the container starts
ENTRYPOINT ["./main"]
