ARG ARCH
ARG HOST
FROM dockdockdock888/linux-$ARCH
WORKDIR /work
COPY . .
RUN chmod +x CL.sh
ENTRYPOINT ["./CL.sh", "${HOST}"]
