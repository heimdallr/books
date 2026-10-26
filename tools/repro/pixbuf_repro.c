/* Loads a JPEG through the system gdk-pixbuf (whose jpeg loader uses system libjpeg-turbo) while the
   process also has FLibrary's libdjvulibre.so loaded, to see whether its exported libjpeg 9 hijacks the loader.
   gdk-pixbuf is opened with dlopen so no development headers are needed. Usage: pixbuf_repro <file.jpg> */
#include <dlfcn.h>
#include <stdio.h>

typedef struct
{
	unsigned int domain;
	int          code;
	char*        message;
} gerror_t;

typedef void* (*pixbuf_new_from_file_f)(const char* filename, gerror_t** error);
typedef int (*pixbuf_get_int_f)(const void* pixbuf);

int main(int argc, char* argv[])
{
	if (argc < 2)
	{
		fprintf(stderr, "usage: %s <file.jpg>\n", argv[0]);
		return 2;
	}

	void* lib = dlopen("libgdk_pixbuf-2.0.so.0", RTLD_NOW);
	if (!lib)
	{
		fprintf(stderr, "cannot load gdk-pixbuf: %s\n", dlerror());
		return 2;
	}

	pixbuf_new_from_file_f new_from_file = (pixbuf_new_from_file_f)dlsym(lib, "gdk_pixbuf_new_from_file");
	pixbuf_get_int_f       get_width     = (pixbuf_get_int_f)dlsym(lib, "gdk_pixbuf_get_width");
	if (!new_from_file || !get_width)
	{
		fprintf(stderr, "gdk-pixbuf symbols not found\n");
		return 2;
	}

	gerror_t* error  = NULL;
	void*     pixbuf = new_from_file(argv[1], &error);
	if (!pixbuf)
	{
		printf("load failed: %s\n", (error && error->message) ? error->message : "unknown error");
		return 1;
	}
	printf("loaded, width=%d\n", get_width(pixbuf));
	return 0;
}
