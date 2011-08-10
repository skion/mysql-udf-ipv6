
# Debian/Ubuntu
PREFIX=/usr

# MacOS/XAMPP
#PREFIX=/Applications/XAMPP/xamppfiles

INCDIR=$(PREFIX)/include
LIBDIR=$(PREFIX)/lib/mysql/plugin

CFLAGS=-O2 -shared -fPIC -I$(INCDIR)

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
