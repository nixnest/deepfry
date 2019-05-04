CC ?= clang
MAGICKINC ?= $(shell find /usr/include -name "ImageMagi*" -type d | head -n1)
MAGICKLIB ?= $(shell find /lib /usr/lib -name "libMagickCore*" | sed 's!.*/!!' | head -n1)

default:
	${CC} -I${MAGICKINC} main.c -lcurl -ltesseract -llept -l:${MAGICKLIB} -O3 -o deepfry