/**
 * mysql_udf_ipv6.c
 *
 * Trivial MySQL functions for IPv6.
 *
 * author: Pieter Ennes
 * email:  labs@watchmouse.com
 * web:    http://labs.watchmouse.com/mysql-udf-ipv6/
 *
 * Copyright (c) 2009 WatchMouse
 *
 * Licensed under the EUPL, Version 1.1 or – as soon they will be approved
 * by the European Commission - subsequent versions of the EUPL (the "Licence");
 * You may not use this work except in compliance with the Licence. You may
 * obtain a copy of the Licence at:
 *
 *   http://ec.europa.eu/idabc/eupl
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the Licence is distributed on an "AS IS" basis,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the Licence for the specific language governing permissions and
 * limitations under the Licence.
 *
 * $Id$
 */

#include <mysql.h>
#include <ctype.h>
#include <string.h>
#include <arpa/inet.h>          // for inet_ntop and inet_pton
#include <netdb.h>

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

my_bool inet6_rlookupn_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void inet6_rlookupn_deinit(UDF_INIT *initid);
char *inet6_rlookupn(UDF_INIT *initid, UDF_ARGS *args, char *result,
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
        strcpy(message, "Wrong arguments to INET6_PTON: provide human readable IPv4 or IPv6 address.");
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
 * Example: SELECT INET6_NTOP(INET6_PTON('1.2.3.4')), INET6_NTOP(INET6_PTON('fe80::219:e3ff:1:9317'));
 *
 * @arg    string   varbinary format ipv4 or ipv6 address
 * @return string   max 46 character presentation string
 */
my_bool inet6_ntop_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT) {
        strcpy(message, "Wrong arguments to INET6_NTOP: provide 4 or 16 byte binary representation.");
        return 1;
    }
    initid->max_length = INET6_ADDRSTRLEN+1; // max length of ipv6 presentation string
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
    if (!inet_ntop(af, args->args[0], result, INET6_ADDRSTRLEN+1)) {
        *null_value = 1;
        return 0;
    }

    *res_length = strlen(result);
    return result;
}


/**
 * inet6_rlookupn()
 *
 * Resolve binary IPv6 or IPv4 address to host name.
 *
 * Example: SELECT INET6_RLOOKUPN(INET6_PTON('2001:4860:a005::68')), INET6_RLOOKUPN(INET6_PTON('64.128.190.61'));
 *
 * @arg    string   varbinary format ipv4 or ipv6 address
 * @return string   resolved host in presentation form
 */
my_bool inet6_rlookupn_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT) {
        strcpy(message, "Wrong arguments to INET6_RLOOKUPN: provide IPv4 or IPv6 address in INET6_PTON form.");
        return 1;
    }
    initid->max_length = NI_MAXHOST;
    initid->maybe_null = 1;
    initid->const_item = 0;
    return 0;
}

void inet6_rlookupn_deinit(UDF_INIT *initid __attribute__((unused)))
{
}

char *inet6_rlookupn(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args, char *result,
        unsigned long *res_length, char *null_value, char *error __attribute__((unused)))
{
    const struct sockaddr_storage sa;
    const char *addr = args->args[0];
    const uint length = args->lengths[0];
    ushort i;

    if (!addr || !length) {
        *null_value = 1;
        return 0;
    }

    if (length == INET6_ADDRLEN) {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *) &sa;

        sa6->sin6_family = AF_INET6;
        for (i = 0; i < length; i++)
            sa6->sin6_addr.s6_addr[i] = addr[i];

    } else if (length == INET_ADDRLEN) {
        struct sockaddr_in *sa4 = (struct sockaddr_in *) &sa;
        char *dest = (char *) &sa4->sin_addr.s_addr;

        sa4->sin_family = AF_INET;
        for (i = 0; i < length; i++)
            dest[i] = addr[i];

    } else {
        *null_value = 1;
        return 0;
    }

    if (getnameinfo((struct sockaddr *) &sa, sizeof(sa), result, NI_MAXHOST, NULL, 0, NI_NAMEREQD)) {
        *null_value = 1;
        return 0;
    }

    *res_length = strlen(result);
    return result;
}

/*
 * TODO:
 * inet6_lookup()
 * inet6_lookupn()
 * inet6_rlookup()
 * inet6_network(ip, cidr/mask)
 * inet6_broadcast(ip, cidr/mask)
 * inet6_numhosts(cidr/mask)
 * inet6_netmask(cidr)
 * inet6_cidr(netmask)
 * inet6_contains(ip, network, cidr/mask)
 */
