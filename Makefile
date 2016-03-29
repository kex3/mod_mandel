default: build

build:
	rm -f *.lo *.slo
	apxs2 -i -a -Wc,-Wall -lm -c mod_mandel.c lodepng.c

launch:
	service apache2 restart
