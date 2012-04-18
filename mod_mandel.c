#include "httpd.h"
#include "http_config.h"

#include "mod_mandel.h"
#include "lodepng.h"
#include <math.h>

module AP_MODULE_DECLARE_DATA mandel_module =
{
	// Only one callback function is provided.  Real
	// modules will need to declare callback functions for
	// server/directory configuration, configuration merging
	// and other tasks.
	STANDARD20_MODULE_STUFF,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	mod_mandel_register_hooks,      /* callback for registering hooks */
};

static int count_char(const char *string, char character)
{
	int i = 0, count = 0;

	for (; string[i] != 0; i++)
	{
		if (string[i] == character)
		{
			count++;
		}
	}

	return count;
}

static void mod_mandel_register_hooks (apr_pool_t *p)
{
	// I think this is the call to make to register a
	// handler for method calls (GET PUT et. al.).
	// We will ask to be last so that the comment
	// has a higher tendency to go at the end.
	ap_hook_handler(mod_mandel_method_handler, NULL, NULL, APR_HOOK_MIDDLE);
}

static int mod_mandel_method_handler (request_rec *r)
{
	// Are we in the "tiles directory"? Do we have the correct path depth (X,Y,Z)?
	if (0 != strcmp(r->filename, "/var/www/tiles") || 3 != count_char(r->path_info, '/'))
	{
		return DECLINED;
	}

	// Strip out the numbers, google maps coordinate style
	long x, y, z, i;
	int j, state = 0, ctr = 0, numlen = 16, tilesize = 256;
	char *numbuf = malloc(numlen);
	long *valbuf;

	for (i = 1; r->path_info[i] != 0; i++)
	{
		if (r->path_info[i] != '/')
		{
			numbuf[ctr++] = r->path_info[i];

			if (ctr == numlen)
			{
				fprintf(stderr, "mod_mandel: number too large (>%d chars)", numlen);
				free(numbuf);
				return DECLINED;
			}
		} else {
			switch(state)
			{
				case 0:
					x = atoi(numbuf);
					break;
				case 1:
					y = atoi(numbuf);
					break;
			}
			
			memset(numbuf, 0, sizeof(numbuf));

			state++;
			ctr = 0;
		}
	}

	z = atoi(numbuf);
	free(numbuf);

	// And with this less-than-pretty hack, we have x,y,z in variables, time to get cracking
	//valbuf = malloc(tilesize * tilesize * sizeof(long));
	unsigned char *imgBuf = (unsigned char *) malloc(tilesize * tilesize * 4);
	size_t bufferSize;

	// Do quick calcuations to verify that this actually works
	long double minRe = -2.0, maxRe = 1.0, minIm = -1.5, maxIm = 1.5;

	// Map out google map tile coordinates to coordinates in set
	long double tileSize = 1.0;
	
	for(i = 0; i < z; i++)
	{
		tileSize = tileSize / 2.0;
	}

	minRe = minRe + (x * tileSize);
	maxRe = minRe + tileSize;
	minIm = minIm + (y * tileSize) + tileSize;
	maxIm = minIm - tileSize;

	long double reFactor = (maxRe - minRe) / ( 1.0 * (tilesize - 1));
	long double imFactor = (maxIm - minIm) / ( 1.0 * (tilesize - 1));

	int maxIter = 128, n;
	int loopx, loopy;

	maxIter += (896 / 22) * z;


	for (loopy = 0; loopy < tilesize; loopy++)
	{
		long double cIm = maxIm - ( loopy * imFactor );

		for (loopx = 0; loopx < tilesize; loopx++)
		{
			long double cRe = minRe + ( loopx * reFactor );

			long double zRe = cRe, zIm = cIm;
			int isInside = 1, n;

			for (n = 0; n < maxIter; n++)
			{
				long double zRe2 = zRe*zRe,
					zIm2 = zIm * zIm;

				if (zRe2 + zIm2 > 4.0)
				{
					isInside = 0;
					break;
				}

				zIm = 2*zRe*zIm + cIm;
				zRe = zRe2 - zIm2 + cRe;
			}

			if (isInside == 0)
			{
				unsigned char red, green, blue;
				double halfIter = maxIter / 2.0;

				if (n < (maxIter / 2.0) - 1.0)
				{
					red = 255 * (n / halfIter);
					green = 0;
					blue = 0;
				} else {
					red = 255;
					green = 255 * ((n - halfIter) / halfIter);
					blue = 255 * ((n - halfIter) / halfIter);
				}

				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 0] = red;
				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 1] = green;
				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 2] = blue;
				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 3] = 255;
			} else {
				//valbuf[x * y] = 0;
				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 0] = 0;
				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 1] = 0;
				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 2] = 0;
				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 3] = 255;
			}
		}
	}

	unsigned char *image;

	LodePNG_Encoder encoder;

	LodePNG_Encoder_init(&encoder);
	encoder.settings.zlibsettings.windowSize = 2048;
	LodePNG_Text_add(&encoder.infoPng.text, "Comment", "Created with LodePNG");

	// naïve anti-alias
/*int antitilesize = tilesize / 2;

	unsigned char *antialiasImgBuf = malloc(antitilesize * antitilesize * 4);

	int offset0 = 0, offset1 = 4, offset2 = tilesize * 4, offset3 = (tilesize + 1) * 4;
	for(y = 0; y < antitilesize; y++)
	{
		for(x = 0; x < antitilesize; x++)
		{
			int pos = (4 * tilesize * y) + (4 * x);

			//for(i = 0; i < 3; i++)
			//{
				antialiasImgBuf[(4 * y * antitilesize) + (4 * x)] =
					(imgBuf[pos + offset0] + imgBuf[pos + offset1] + imgBuf[pos + offset2] + imgBuf[pos + offset3]) / 4;
				antialiasImgBuf[1 + (4 * y * antitilesize) + (4 * x)] = 0;
				antialiasImgBuf[2 + (4 * y * antitilesize) + (4 * x)] = 0;
				antialiasImgBuf[3 + (4 * y * antitilesize) + (4 * x)] = 255;


			//	pos++;
			//}
				
		}
	}*/

	LodePNG_Encoder_encode(&encoder, &image, &bufferSize, imgBuf, tilesize, tilesize);

	if(encoder.error)
	{
		fprintf(stderr, "Encoder error %u: %s\n", encoder.error, LodePNG_error_text(encoder.error));
	}

	r->content_type = "image/png";
	ap_rwrite(image, bufferSize, r);
	ap_set_content_length(r, bufferSize);

	LodePNG_Encoder_cleanup(&encoder);
	free(imgBuf);
	free(image);

	return OK;
}
