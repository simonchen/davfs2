ARG ARCH
ARG HOST
FROM dockdockdock888/linux-$ARCH
ENV HOST=$HOST
WORKDIR /work
COPY . .
RUN chmod +x CL.sh
ENTRYPOINT ["./CL.sh", "$HOST"]
