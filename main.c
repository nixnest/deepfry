#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>//the 4 mighty C standard headers
#include <curl/curl.h>//download things
#include <leptonica/allheaders.h>//need that to load images to a format that tesseract likes to eat
#include <tesseract/capi.h>//api for detecting stuff
#define MAGICKCORE_MAGICK_CONFIG_H //won't compile otherwise on Debian, TODO: change when fixed
#include <magick/MagickCore.h>//for image manipulation
#include <magick/opencl.h>//opencl support
#include "b.h"//B image data, gives 'int b_image_size' and 'char b_image[]'

size_t percentage = 5;

char* image_data;//our precious image ☭
size_t data_size;
size_t image_width;
size_t image_height;

static size_t downloaded_chunk(void* contents, size_t size, size_t nmemb)//here we take care of download process
{//some servers are dumb af and send chunked files, so program is responsible for joining them
	size_t real_size = size * nmemb;
	image_data = realloc(image_data, data_size + real_size + 1);
	if(!image_data)//no memory left
	{
		fputs("Could not allocate memory!\n", stderr);
		fflush(stderr);
		return 0;
	}
	memcpy(&(image_data[data_size]), contents, real_size);
	data_size += real_size;
	image_data[data_size] = 0;
	return real_size;
}

uint64_t hasher(char* data, size_t size)
{
	uint64_t hash = 14695981039346656037U;//magic offset constant
	for(size_t i = 0; i < size; i++)
	{
		hash ^= data[i];
		hash *= 1099511628211U;//second magic constant (must be prime)
	}
	return hash;
}

uint64_t rand_state;
uint64_t rand_seq;

uint32_t rand_gen()//fricking secure and fast pseudo rand gen
{
    uint64_t oldstate = rand_state;
    rand_state = oldstate * 6364136223846793005ULL + rand_seq;
    uint32_t xorshifted = ((oldstate >> 18) ^ oldstate) >> 27;
    uint32_t rot = oldstate >> 59;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

void rand_seed(uint64_t seed)//actually two separete seeds (such an algo)
{
	uint64_t state; uint64_t seq;
	state = seed;
	seq = seed*3/5;//second seed
    rand_state = 0;
    rand_seq = (seq << 1) | 1;
    rand_gen();
    rand_state += state;
    rand_gen();
}

static const char* str1 = "http\0\0\0";//constants used for URL check
static const char* str2 = ".jpg\0\0\0";//max 7 characters
static const char* str3 = ".png\0\0\0";
static const char* str4 = ".jpeg\0\0";

static const char* str5 = "\xFF\xFF\xFF\xFF\0\0\0";//masks to make sure no overflow is collected
static const char* str6 = "\xFF\xFF\xFF\xFF\xFF\0\0";

int main(int argc, char **argv)
{
	uint64_t prefix = *((uint64_t*) str1);
	uint64_t suffix1 = *((uint64_t*) str2);
	uint64_t suffix2 = *((uint64_t*) str3);
	uint64_t suffix3 = *((uint64_t*) str4);
	uint64_t mask1 = *((uint64_t*) str5);
	uint64_t mask2 = *((uint64_t*) str6);

	char* image_location;
	long int location_length;

	if(argc > 1)
	{
		image_location = argv[1];
		location_length = strlen(argv[1]);
	}
	else
	{
		fputs("File location not specified!\n", stderr);
		fflush(stderr);
		return 0;
	}

	if//check if location is an URL
	(
		(*((uint64_t*) image_location) & mask1) == prefix &&
		(
			(*((uint64_t*) (image_location + location_length - 4)) & mask1) == suffix1 ||
			(*((uint64_t*) (image_location + location_length - 4)) & mask1) == suffix2 ||
			(*((uint64_t*) (image_location + location_length - 5)) & mask2) == suffix3
		)
	)
	{//do curl stuff
		CURL *curl_handle;
		CURLcode res;

		image_data = malloc(1);
		data_size = 0;

		curl_global_init(CURL_GLOBAL_ALL);
		curl_handle = curl_easy_init();
		curl_easy_setopt(curl_handle, CURLOPT_URL, image_location);
		curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, downloaded_chunk);
		curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "MSIE/10.0");//hopefuly we get treated as IE
		//thus giving us longer time to download an image (also some servers need that var because reasons)
		res = curl_easy_perform(curl_handle); 
		if(res != CURLE_OK)
		{
			fputs("Downloading error:\n", stderr);
			fputs(curl_easy_strerror(res), stderr);
			fputs("\n", stderr);
			fflush(stderr);
			return 0;
		}
		curl_easy_cleanup(curl_handle);
		curl_global_cleanup();
	}
	else
	{//do file stuff
		FILE *file = fopen(image_location, "rb");
		if(!file)
		{
			fputs("Could not open file!\n", stderr);
			fflush(stderr);
			return 0;
		}
		fseek(file, 0, SEEK_END);
		long int fsize = ftell(file);
		fseek(file, 0, SEEK_SET);

		image_data = malloc(fsize + 1);
		if(!image_data)
		{//no memory left
			fputs("Could not allocate memory!\n", stderr);
			fflush(stderr);
			return 0;
		}
		fread(image_data, 1, fsize, file);
		fclose(file);
		image_data[fsize] = 0;
		data_size = fsize;
	}
	//now we have fricking image in memory, time to make magick happen
	//part 1: B characters
	rand_seed(hasher(image_data, data_size));//prepare the random generator
	PIX* image = pixReadMem((l_uint8*)image_data, data_size);
	TessBaseAPI* tess_api = TessBaseAPICreate();
	TessBaseAPIInit3(tess_api, 0, "eng");
	TessBaseAPISetImage2(tess_api, image);
	struct Boxa* dimensions = TessBaseAPIGetComponentImages(tess_api, RIL_SYMBOL, 1, 0, 0);
	struct Box* dimension;
	//Boxa is a struct with box arrays
	//'n' is an element count (32 bit int)
	//'box' is an array of box pointers
	//I know they are private members but C is about data, not objects
	//less function calls = faster
	//Box has members 'x' 'y' 'w' 'h' which are 32 bit ints
	TessBaseAPIDelete(tess_api);
	image_width = pixGetWidth(image);
	image_height = pixGetHeight(image);
	pixDestroy(&image);//get the memory back!

	MagickCoreGenesis(*argv,MagickTrue);//start magick
	ExceptionInfo* exception = AcquireExceptionInfo();
	MagickCLEnv clEnv = GetDefaultOpenCLEnv();
	if(InitOpenCLEnv(clEnv,exception) == MagickFalse)
	{
		fputs("Could not initialise OpenCL, using basic functionality.\n", stderr);
		fflush(stderr);
	}
	else
	{
		fputs("Program does not support OpenCL, using basic functionality.\n", stderr);
		fflush(stderr);
		//Start opencl stuff
		//TODO when i figure out this crap
	}
	ImageInfo* base_info = CloneImageInfo(0);
	ImageInfo* B_info = CloneImageInfo(0);
	Image* base = BlobToImage(base_info, image_data, data_size, exception);
	Image* B = BlobToImage(B_info, b_image, b_image_size, exception);
	Image* parser_image;
	RectangleInfo rectangle;
	rectangle.x = 0;
	rectangle.y = 0;
	OffsetInfo point;
	//2% (each 100 elements should get 2), dividing by 50
	uint32_t percent_n = (dimensions->n)*percentage/100;
	if((!percent_n) && dimensions->n > 1)
	{//image without :B: has no meaning or value, no matter how small
		if(rand_gen()&1)//however if random number is even then God dislikes this image
		{
			percent_n = 1;
		}
	}
	//need to remember for cleaning up later
	uint32_t* found_index_stack = malloc(percent_n * sizeof(uint32_t));
	uint32_t index;
	for(uint32_t i = 0; i < percent_n; i++)//2% of all results
	{
		do
		{
			index = rand_gen() % dimensions->n;//get deterministic index
			dimension = dimensions->box[index];
		}
		while(!dimension->refcount);//make sure we didn't use it before
		found_index_stack[i] = index;
		dimension->refcount = 0;//it is not used in program nevertheless, so i can use it for repetition checks
		//parse the B!
		point.x = dimension->x;
		point.y = dimension->y;
		rectangle.width = dimension->w;
		rectangle.height = dimension->h;
		parser_image = AdaptiveResizeImage(B, rectangle.width, rectangle.height, exception);
		CopyImagePixels(base, parser_image, &rectangle, &point, exception);//parse on location
		DestroyImage(parser_image);
	}
	for(uint32_t i = 0; i < percent_n; i++)//if i won't get it back to normal, api might go nuts
		dimensions->box[found_index_stack[i]]->refcount = 1;//tesseract api created one reference, thus 1
	free(found_index_stack);
	boxaDestroy(&dimensions);
	DestroyImage(B);//precious memory
	//part 2 - deepfry
	//cas:
	//most prob can be replaced by single image function, TODO
	parser_image = LiquidRescaleImage(base, image_width/2, image_height/2, 1, 1, exception);
	DestroyImage(base);
	base = LiquidRescaleImage(parser_image, image_width, image_height, 1, 1, exception);
	DestroyImage(parser_image);
	//modulate:
	ModulateImage(base, "50,200");// ← super bloated function that should be replaced
	//here is unfinished direct implementation that should be faster
	//to remember: update colormaps, then pixels (maybe only pixels if colormap won't be changed later)
	/*PixelPacket* colors = base->colormap;
	double hue, saturation, lightness;
	for(size_t i = 0; i < base->colors; i++)
	{
		ConvertRGBToHSL(colors[i].red, colors[i].green, colors[i].blue, &hue, &saturation, &lightness);
		hue+=fmod((50-100.0),200.0)/200.0;//50%
		saturation*=0.01*200;//200%
		//lightness*=0.01*100;
		ConvertHSLToRGB(hue, saturation, lightness, &colors[i].red, &colors[i].green, &colors[i].blue);
	}*/
	//emboss:
	//same what happens with modulation; colormap is changed
	parser_image = EmbossImage(base, 1.1, 0, exception);
	DestroyImage(base);
	//gaussian blur:
	SetImageArtifact(parser_image, "attenuate", "0.5");
	base = AddNoiseImage(parser_image, GaussianNoise, exception);
	DestroyImage(parser_image);
	//finally export
	WriteImages(base_info, base, "deepfry_output.png", exception);
	DestroyImage(base);
	MagickCoreTerminus();//we done
	return 0;
}