ARG ARCH
FROM dockcross/linux-$ARCH
RUN chmod +x dockcross && ./dockcross bash
