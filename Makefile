CFLAGS=-O2 -shared -fPIC
LIBDIR=/usr/lib/mysql/plugin/

# pre 5.0.67
#LIBDIR=/usr/local/lib/

all: mysql_udf_ipv6.so mysql_udf_idna.so

mysql_udf_ipv6.so: mysql_udf_ipv6.c
	gcc $(CFLAGS) -o $@ $+

mysql_udf_idna.so: mysql_udf_idna.c
	gcc $(CFLAGS) -lidn -o $@ $+

install: mysql_udf_ipv6.so mysql_udf_idna.so
	cp -f $? $(LIBDIR)

uninstall: mysql_udf_ipv6.so mysql_udf_idna.so
	cd $(LIBDIR) && rm -f $+

clean:
	rm -f *.so
