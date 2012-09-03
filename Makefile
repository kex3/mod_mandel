default: build

build:
	rm -f *.lo *.slo
	apxs2 -i -a -Wc,-Wall -lm -c mod_mandel.c lodepng.c

launch:
	/etc/init.d/varnish stop
	rm /var/lib/varnish/mandel.kex3.com/*
	/etc/init.d/varnish start
	/etc/init.d/apache2 restart

