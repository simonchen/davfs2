ARG ARCH
FROM dockcross/linux-$ARCH
RUN apt-get update && 
    apt-get -y install gcc mono-mcs 
