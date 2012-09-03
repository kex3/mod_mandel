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

inline static int iterate(long double cr, long double ci, long double K, long double f, int MaxIt) {
	long double Cr, Ci, I = 0, R = 0, I2 = I * I, R2 = R * R, Dr = 0, Di = 0, D;
	int n = 0;

	if (f == 0) {
		Cr = cr;
		Ci = ci;
	} else {
		Cr = ci;
		Ci = cr;
	}

	do {
		D = 2 * (R * Dr - I * Di) + 1;
		Di = 2 * (R * Di + I * Dr);
		Dr = D;
		I = (R + R) * I + Ci;
		R = R2 - I2 + Cr;
		R2 = R * R;
		I2 = I * I;
		n++;
	} while ((R2 + I2 < 100.) && (n < MaxIt));

	if (n == MaxIt)
		return 0; // interior

	else { // boundary and exterior
		R = -K * log( log( R2 + I2 ) * sqrt((R2 + I2) / (Dr * Dr + Di * Di))); // compute distance
		if (R < 0) {
			R = 0;
		}

		return (int) (R / 8);
	};
}

/*static inline long calculate(long double cIm, long double cRe, long maxIter, long double bailout)
{
	long n;
	long double zRe = cRe, zIm = cIm, zDe = 0.0;

	// z'n+1 = 2z'nzn

	for (n = 0; n < maxIter; n++)
	{
		long double zRe2 = zRe*zRe,
			zIm2 = zIm * zIm;

		if (zRe2 + zIm2 >= bailout)
		{
			return n;
		}

		zDe = 2 * zDe * zRe;
		zIm = 2*zRe*zIm + cIm;
		zRe = zRe2 - zIm2 + cRe;
	}

	return 0;
}*/

static int mod_mandel_method_handler (request_rec *r)
{
	// Are we in the "tiles directory"? Do we have the correct path depth (X,Y,Z)?
	if (0 != strcmp(r->filename, "/var/www/tiles") || 3 != count_char(r->path_info, '/'))
	{
		return DECLINED;
	}

	// Strip out the numbers, google maps coordinate style
	long long x = 0, y = 0, z = 0, i;
	int tilesize = 256;

	sscanf(r->path_info, "/%lld/%lld/%lld", &x, &y, &z);

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

	long maxIter = 256;
	int loopx, loopy;



	for (loopy = 0; loopy < tilesize; loopy++)
	{
		long double cIm = maxIm - ( loopy * imFactor );

		for (loopx = 0; loopx < tilesize; loopx++)
		{
			long double cRe = minRe + ( loopx * reFactor );
			long n;

			unsigned long _r = 0, _g = 0, _b = 0;

			int innerX, innerY;

			n = iterate(cRe, cIm, 1024, 0, maxIter);

			//if (n > 0) {
				for (innerX = -1; innerX < 2; innerX++) {
					for (innerY = -1; innerY < 2; innerY++) {
						int _n;

						_n = iterate(cRe + ((innerX * 0.5) * reFactor), cIm  + ((innerY * 0.5) * imFactor), 1024, 0, maxIter);

						_n = _n % 1024;

						_r += pal_red[_n];
						_g += pal_green[_n];
						_b += pal_blue[_n];
					}
				}

				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 0] = (int) (_r / 9L);
				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 1] = (int) (_g / 9L);
				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 2] = (int) (_b / 9L);


				/*imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 0] = (int) (pal_red[n]);
				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 1] = (int) (pal_green[n]);
				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 2] = (int) (pal_blue[n]);*/

				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 3] = 255;
			/*} else {
				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 0] = 0;
				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 1] = 0;
				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 2] = 0;
				imgBuf[(4 * tilesize * loopy) + (4 * loopx) + 3] = 255;
			}*/
		}
	}

	unsigned char *image;

	LodePNG_Encoder encoder;

	LodePNG_Encoder_init(&encoder);
	encoder.settings.zlibsettings.windowSize = 128;

	char *footer;
	footer = malloc(128);

	sprintf(footer, "Created with LodePNG: (%lld / %lld / %lld)", x, y, z);

	LodePNG_Text_add(&encoder.infoPng.text, "Comment", footer);
	LodePNG_Encoder_encode(&encoder, &image, &bufferSize, imgBuf, tilesize, tilesize);

	if(encoder.error)
	{
		fprintf(stderr, "Encoder error %u: %s\n", encoder.error, LodePNG_error_text(encoder.error));
		return DECLINED;
	}

	r->content_type = "image/png";
	ap_rwrite(image, bufferSize, r);
	ap_set_content_length(r, bufferSize);

	LodePNG_Encoder_cleanup(&encoder);
	free(imgBuf);
	free(image);

	return OK;
}
