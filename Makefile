INC=-I /usr/include/mysql
SRC=mysql_udf_ipv6.c
OBJ=mysql_udf_ipv6.so
# pre 5.0.67
#LIBDIR=/usr/local/lib/
# post 5.0.67
LIBDIR=/usr/lib/mysql/plugin/

mysql_udf_ipv6.so: $(SRC)
	gcc -O2 $(INC) -shared -o $(OBJ) -fPIC $(SRC)

install:
	cp -f $(OBJ) $(LIBDIR)

clean:
	rm *.so
