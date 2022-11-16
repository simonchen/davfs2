ARG ARCH
ARG HOST
FROM dockdockdock888/linux-$ARCH
WORKDIR /work
COPY . .
RUN chmod +x CL.sh
RUN echo $HOST
ENTRYPOINT ["./CL.sh", $HOST]
