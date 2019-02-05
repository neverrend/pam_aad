CC = gcc
INSTALL = /usr/bin/install
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA = ${INSTALL} -m 644

prefix = ""
exec_prefix = ${prefix}

sysconfdir = /etc

SRC_DIR = src

COMMON_SRC = $(SRC_DIR)/rest.c
COMMON_SRC += $(SRC_DIR)/utils.c

NSS_SRC = $(COMMON_SRC) $(SRC_DIR)/libnss_aad.c
PAM_SRC = $(COMMON_SRC) $(SRC_DIR)/jwt.c $(SRC_DIR)/pam_aad.c

COMMON_LIBS = -lssl -lcrypto
NSS_LIBS = $(COMMON_LIBS)
PAM_LIBS = $(COMMON_LIBS) -lpam -lm -ljwt

COMMON_LDFLAGS = -fPIC -fno-stack-protector -Wall -shared 
COMMON_LDFLAGS += -Wl,-x -Wl,--strip-debug -Wl,--build-id=none

NSS_LDFLAGS = $(COMMON_LDFLAGS) -Wl,-soname,libnss_aad.so.2
PAM_LDFLAGS = $(COMMON_LDFLAGS) -fno-version

LIB_DIR = $(prefix)/lib
PAM_DIR = $(prefix)/lib/security

all: libnss_aad pam_aad

libnss_aad: $(NSS_SRC)
	${CC} ${CFLAGS} ${LDFLAGS} ${NSS_LDFLAGS} -o libnss_aad.so.2 ${NSS_SRC}

pam_aad: $(PAM_SRC)
	${CC} ${CFLAGS} ${LDFLAGS} ${PAM_LDFLAGS} -o pam_aad.so ${PAM_SRC}

install:
	${INSTALL_DATA} libnss_aad.so.2 ${LIB_DIR}
	${INSTALL_DATA} pam_aad.so ${PAM_DIR}

clean: 
	rm -f libnss_aad.so.2 pam_aad.so
