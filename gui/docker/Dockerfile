# Use a base image with the desired Linux distribution and version
FROM ubuntu:latest

# Set the working directory inside the container
WORKDIR /usr/src/app

# Copy the source code files into the image
COPY server.cpp game.h ./

# Install necessary build tools and dependencies
RUN apt-get update && \
    apt-get install -y g++

# Compile the C++ application
RUN g++ -o server server.cpp

# Specify the command to run your application
CMD ["./server"]
