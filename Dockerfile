FROM ubuntu:18.04

WORKDIR /usr/src/pam_aad
COPY . /usr/src/pam_aad

RUN apt update && apt upgrade -y && \
    apt install -y automake build-essential git libjwt-dev libpam0g-dev libssl-dev libtool

RUN autoreconf --install && \
    ./configure && \
    make

#WORKDIR /usr/src
#RUN git clone https://github.com/donapieppo/libnss-ato && \
#    cd libnss-ato && make

FROM ubuntu:18.04
COPY --from=0 /usr/src/pam_aad/.libs/pam_aad.so /lib/x86_64-linux-gnu/security/
#COPY --from=0 /usr/src/libnss-ato/libnss_ato.so.2 /lib/x86_64-linux-gnu/libnss_ato-2.3.6.so
RUN apt update && apt upgrade -y && \
    apt install -y openssh-server
