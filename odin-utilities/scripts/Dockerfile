#
# Ubuntu Dockerfile
#
# https://github.com/fgg89/
#

# Pull base image
FROM ubuntu:14.04
MAINTAINER Fran Gonzalez

# Install
RUN apt-get update && apt-get install -y wget build-essential flex bison
RUN wget http://www.tcpdump.org/release/libpcap-1.4.0.tar.gz
RUN tar -xf libpcap-1.4.0.tar.gz && cd libpcap-1.4.0 && ./configure && make && make install
RUN ldconfig

# Add files

# Set environment variables.
ENV HOME /root

# Define working directory.
WORKDIR /root

# Define default command.
#CMD ["bash", "/root/spring/shared/scripts/start-click.sh"]
CMD ["bash"]
