ARG ARCH
FROM dockdockdock888/linux-$ARCH
WORKDIR /work
COPY . .
ENTRYPOINT ["./CL.sh"]
