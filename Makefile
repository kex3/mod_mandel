default: build launch

build:
	apxs2 -i -a -Wc,-Wall -c mod_mandel.c lodepng.c

launch:
	/etc/init.d/apache2 restart
	/etc/init.d/varnish restart
