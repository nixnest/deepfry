# deepfry
*yeet, very cool*\
Deepfry is a C implementation of image distortion process

## Dependencies
To actually compile it you need to have these libraries and corresponding headers:\
	-`libc`\
	-`libcurl`\
	-`liblept` (a dependency of libtesseract, however sometimes you might need to install it separately)\
	-`libtesseract`\
	-`libmagickcore`\
	-`liblqr`

## Building
Make sure you have base development package installed (`base-devel` is the name in most cases) and run `make` inside the project folder. Linux-only (with enough luck you might succeed on other UNIX-like systems).

### Troubleshooting
If you encounter problems with ImageMagick please consider specifying those `make` variables:\
`MAGICKINC` - path to an include directory containing 'magick' named catalogue\
`MAGICKLIB` - full name of libMagickCore library\
Example:\
	`make MAGICKINC=/usr/include/ImageMagick MAGICKLIB=libMagickCore.so`

## Running
Built `deepfry` binary can be moved anywhere without disturbing how well it works. To use it invoke it with the first argument being a http or https url or local path to an image file. Output is saved as `deepfry_output.png` in current working directory.\
Examples:\
	`./deepfry https://www.kernel.org/theme/images/logos/tux.png`\
	`./deepfry ./myimage.jpg`