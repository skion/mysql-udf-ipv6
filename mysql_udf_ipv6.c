/**
 * mysql-udf-ipv6
 *
 * Trivial MySQL functions for IPv6.
 *
 * Copyright (c) 2009 WatchMouse
 *
 * author: Pieter Ennes
 * email:  labs@watchmouse.com
 * web:    http://labs.watchmouse.com/mysql-udf-ipv6/
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * $id$
 */

#include <mysql.h>
#include <ctype.h>
#include <string.h>
#include <arpa/inet.h>          // for inet_ntop and inet_pton
#include <netdb.h>
#define strmov(a,b) strcpy(a,b)

// 4 and 16 byte address lengths
#define INET_ADDRLEN (sizeof(struct in_addr))
#define INET6_ADDRLEN (sizeof(struct in6_addr))


my_bool inet6_pton_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void inet6_pton_deinit(UDF_INIT *initid);
char *inet6_pton(UDF_INIT *initid, UDF_ARGS *args, char *result,
        unsigned long *length, char *null_value, char *error);

my_bool inet6_ntop_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void inet6_ntop_deinit(UDF_INIT *initid);
char *inet6_ntop(UDF_INIT *initid, UDF_ARGS *args, char *result,
        unsigned long *length, char *null_value, char *error);

my_bool inet6_gethostbyname_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void inet6_gethostbyname_deinit(UDF_INIT *initid);
char *inet6_gethostbyname(UDF_INIT *initid, UDF_ARGS *args, char *result,
        unsigned long *length, char *null_value, char *error);


/**
 * inet6_pton()
 *
 * Convert IPv4 or IPv6 presentation string to VARBINARY(16) representation.
 *
 * Example: SELECT INET6_PTON('1.2.3.4'), INET6_PTON('fe80::219:e3ff:1:9317');
 *
 * @arg    string   human readable ipv4 or ipv6 address
 * @return string   4 or 16 byte binary string
 */
my_bool inet6_pton_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT) {
        strmov(message, "Wrong arguments to inet6_pton: provide human readable ipv4 or ipv6 address");
        return 1;
    }
    initid->max_length = INET6_ADDRLEN; // # bytes in INET6
    initid->maybe_null = 1;
    initid->const_item = 0;
    return 0;
}

void inet6_pton_deinit(UDF_INIT *initid __attribute__((unused)))
{
}

char *inet6_pton(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args, char *result,
        unsigned long *res_length, char *null_value, char *error __attribute__((unused)))
{
    char temp[INET6_ADDRSTRLEN + 1];
    char buff[INET6_ADDRLEN];
    uint length;
    int af;

    if (!args->args[0] || !(length = args->lengths[0])) {
        *null_value = 1;
        return 0;
    }

    // cannot assume null-terminated string according to manual
    if (length >= sizeof(temp))
        length = sizeof(temp) - 1;
    memcpy(temp, args->args[0], length);
    temp[length] = 0;

    // address family
    if (strpbrk(temp, ".")) {
        af = AF_INET;
        length = INET_ADDRLEN;
    } else {
        af = AF_INET6;
        length = INET6_ADDRLEN;
    }

    // convert
    if (inet_pton(af, temp, result) != 1) {
        *null_value = 1;
        return 0;
    }
    *res_length = length;
    return result;
}

/**
 * inet6_ntop()
 *
 * Convert IPv4 or IPv6 VARBINARY(16) format to presentation string.
 *
 * Example: SELECT NTOP(PTON('1.2.3.4')), NTOP(PTON('fe80::219:e3ff:1:9317'));
 *
 * @arg    string   varbinary format ipv4 or ipv6 address
 * @return string   max 46 character presentation string
 */
my_bool inet6_ntop_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT) {
        strmov(message, "Wrong arguments to inet6_ntop: provide 4 or 16 byte binary representation");
        return 1;
    }
    initid->max_length = INET6_ADDRSTRLEN; // max length of ipv6 presentation string
    initid->maybe_null = 1;
    initid->const_item = 0;
    return 0;
}

void inet6_ntop_deinit(UDF_INIT *initid __attribute__((unused)))
{
}

char *inet6_ntop(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args, char *result,
        unsigned long *res_length, char *null_value, char *error __attribute__((unused)))
{
    int af;

    if (!args->args[0] || !(args->lengths[0])) {
        *null_value = 1;
        return 0;
    }

    // address family
    switch (args->lengths[0]) {
    case INET_ADDRLEN:
        af = AF_INET;
        break;
    case INET6_ADDRLEN:
        af = AF_INET6;
        break;
    default:
        *null_value = 1;
        return 0;
    }

    // convert
    if (!inet_ntop(af, args->args[0], result, INET6_ADDRSTRLEN + 1)) {
        *null_value = 1;
        return 0;
    }

    *res_length = strlen(result);
    return result;
}


/**
 * inet6_gethostbyname()
 *
 * Resolve IPv6 or IPv4 host to IP address.
 *
 * Example: SELECT INET6_GETHOSTBYNAME('ipv6.google.com'), INET6_GETHOSTBYNAME('fe80::219:e3ff:1:9317');
 *
 * @arg    string   host name
 * @return string   resolved IP in presentation form
 */
my_bool inet6_gethostbyname_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT) {
        strmov(message, "Wrong arguments to inet6_gethostbyname: provide human readable ipv4 or ipv6 address");
        return 1;
    }
    initid->max_length = INET6_ADDRSTRLEN; // # bytes in INET6
    initid->maybe_null = 1;
    initid->const_item = 0; // @@@ constant here?
    return 0;
}

void inet6_gethostbyname_deinit(UDF_INIT *initid __attribute__((unused)))
{
}

char *inet6_gethostbyname(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args, char *result,
        unsigned long *res_length, char *null_value, char *error __attribute__((unused)))
{
    struct hostent he, *heptr;          // malloc these in init
    char temp[128];
    char buff[256];
    uint length;
    int af, errno;

    if (!args->args[0] || !(length = args->lengths[0])) {
        *null_value = 1;
        return 0;
    }

    // cannot assume null-terminated string according to manual
    if (length >= sizeof(temp))
        length = sizeof(temp) - 1;
    memcpy(temp, args->args[0], length);
    temp[length] = 0;

    // reentrant
    //    extern int gethostbyname2_r (__const char *__restrict __name, int __af,
    //                                 struct hostent *__restrict __result_buf,
    //                                 char *__restrict __buf, size_t __buflen,
    //                                 struct hostent **__restrict __result,
    //                                 int *__restrict __h_errnop);

    // try to resolve ipv6 host
    gethostbyname2_r(temp, AF_INET6, &he, buff, sizeof(buff), &heptr, &errno);
    if (errno) {
        // fallback to ipv4
        gethostbyname2_r(temp, AF_INET, &he, buff, sizeof(buff), &heptr, &errno);
    }

    if (errno || !inet_ntop(he.h_addrtype, he.h_addr_list[0], result, INET6_ADDRSTRLEN + 1)) {
        *null_value = 1;
        return 0;
    }
    *res_length = strlen(result);
    return result;
}

/*
 * TODO:
 * inet6_getaddrbyname
 * inet6_network(ip, cidr/mask)
 * inet6_broadcast(ip, cidr/mask)
 * inet6_numhosts(cidr/mask)
 * inet6_netmask(cidr)
 * inet6_cidr(netmask)
 * inet6_contains(ip, network, cidr/mask)
 * inet6_expanded(ip) expanded v6 notation
 */
