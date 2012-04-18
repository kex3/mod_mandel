default: build launch

build:
	apxs2 -i -a -c mod_mandel.c lodepng.c

launch:
	/etc/init.d/apache2 restart
