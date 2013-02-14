/**
 * mysql_udf_ipv6.c
 *
 * Trivial MySQL functions for IPv6.
 *
 * author: Pieter Ennes
 * email:  labs@watchmouse.com
 * web:    http://labs.watchmouse.com/mysql-udf-ipv6/
 *
 * Copyright (c) 2009-2011 WatchMouse
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
 *
 */

#include <ctype.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>          // for inet_ntop and inet_pton
#include <netdb.h>
#include <limits.h>

#include <mysql/mysql.h>

// 4 and 16 byte address lengths
#define INET_ADDRLEN (sizeof(struct in_addr))
#define INET6_ADDRLEN (sizeof(struct in6_addr))

#define min(x, y)       ((x) < (y) ? (x) : (y))
#define max(x, y)       ((x) > (y) ? (x) : (y))

my_bool inet6_pton_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void inet6_pton_deinit(UDF_INIT *initid);
char *inet6_pton(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length,
        char *null_value, char *error);

my_bool inet6_ntop_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void inet6_ntop_deinit(UDF_INIT *initid);
char *inet6_ntop(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length,
        char *null_value, char *error);

my_bool inet6_aton_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void inet6_aton_deinit(UDF_INIT *initid);
char *inet6_aton(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length,
        char *null_value, char *error);

my_bool inet6_ntoa_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void inet6_ntoa_deinit(UDF_INIT *initid);
char *inet6_ntoa(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length,
        char *null_value, char *error);

my_bool inet6_mask_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void inet6_mask_deinit(UDF_INIT *initid);
char *inet6_mask(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length,
        char *null_value, char *error);

my_bool inet6_lookup_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void inet6_lookup_deinit(UDF_INIT *initid);
char *inet6_lookup(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length,
        char *null_value, char *error);

my_bool inet6_rlookup_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void inet6_rlookup_deinit(UDF_INIT *initid);
char *inet6_rlookup(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length,
        char *null_value, char *error);


/**
 * As of MySQL 5.6.3 the same functionality is provided via native INET6_NTOA() and INET6_ATON() functions.
 * I've added aliases here to create a migration path from this module to the identical functions in future MySQL.
 */
#ifndef SKIP_MYSQL_COMPAT
my_bool inet6_aton_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    return inet6_pton_init(initid, args, message);
}

void inet6_aton_deinit(UDF_INIT *initid __attribute__((unused)))
{
}

char *inet6_aton(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args, char *result, unsigned long *res_length,
        char *null_value, char *error __attribute__((unused)))
{
    return inet6_pton(initid, args, result, res_length, null_value, error);
}

my_bool inet6_ntoa_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    return inet6_ntop_init(initid, args, message);
}

void inet6_ntoa_deinit(UDF_INIT *initid __attribute__((unused)))
{
}

char *inet6_ntoa(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args, char *result, unsigned long *res_length,
        char *null_value, char *error __attribute__((unused)))
{
    return inet6_ntop(initid, args, result, res_length, null_value, error);
}

#endif /* SKIP_MYSQL_COMPAT */

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
    if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT)
    {
        strcpy(message,
                "Wrong arguments to INET6_PTON: provide human readable IPv4 or IPv6 address.");
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

char *inet6_pton(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args, char *result, unsigned long *res_length,
        char *null_value, char *error __attribute__((unused)))
{
    char temp[INET6_ADDRSTRLEN + 1];
    uint length;
    int af;

    if (!args->args[0] || !(length = args->lengths[0]))
    {
        *null_value = 1;
        return 0;
    }

    // cannot assume null-terminated string according to manual
    if (length >= sizeof(temp))
        length = sizeof(temp) - 1;
    memcpy(temp, args->args[0], length);
    temp[length] = 0;

    // address family
    if (strpbrk(temp, ":"))
    {
        af = AF_INET6;
        length = INET6_ADDRLEN;
    }
    else
    {
        af = AF_INET;
        length = INET_ADDRLEN;
    }

    // convert
    if (inet_pton(af, temp, result) != 1)
    {
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
    if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT)
    {
        strcpy(message,
                "Wrong arguments to INET6_NTOP: provide 4 or 16 byte binary representation.");
        return 1;
    }
    initid->max_length = INET6_ADDRSTRLEN + 1; // max length of ipv6 presentation string
    initid->maybe_null = 1;
    initid->const_item = 0;
    return 0;
}

void inet6_ntop_deinit(UDF_INIT *initid __attribute__((unused)))
{
}

char *inet6_ntop(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args, char *result, unsigned long *res_length,
        char *null_value, char *error __attribute__((unused)))
{
    int af;

    if (!args->args[0] || !(args->lengths[0]))
    {
        *null_value = 1;
        return 0;
    }

    // address family
    switch (args->lengths[0])
    {
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
    if (!inet_ntop(af, args->args[0], result, INET6_ADDRSTRLEN + 1))
    {
        *null_value = 1;
        return 0;
    }

    *res_length = strlen(result);
    return result;
}

/**
 * inet6_mask()
 *
 * Convert IPv4 or IPv6 VARBINARY(16) format to presentation string.
 *
 * Example: SELECT INET6_NTOP(INET6_PTON('1.2.3.4')), INET6_NTOP(INET6_PTON('fe80::219:e3ff:1:9317'));
 *
 * @arg    string   varbinary format ipv4 or ipv6 address
 * @return string   max 46 character presentation string
 */
my_bool inet6_mask_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    if (args->arg_count != 2 || args->arg_type[0] != STRING_RESULT || args->arg_type[1] != INT_RESULT)
    {
        strcpy(message,
                "Wrong arguments to INET6_MASK: provide 4 or 16 byte binary representation and integer mask.");
        return 1;
    }
    initid->max_length = INET6_ADDRLEN; // max length of ipv6 binary string
    initid->maybe_null = 1;
    initid->const_item = 0;
    return 0;
}

void inet6_mask_deinit(UDF_INIT *initid __attribute__((unused)))
{
}

char *inet6_mask(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args, char *result, unsigned long *res_length,
        char *null_value, char *error __attribute__((unused)))
{
    unsigned long length = args->lengths[0];
    long long mask = *((long long *) args->args[1]);
    unsigned char mask8, i;

    if (!args->args[0] || !length)
    {
        *null_value = 1;
        return 0;
    }

    // check mask parameter
    if (!args->args[1] || !(args->lengths[1]) || mask < 0 || mask > length * CHAR_BIT)
    {
        *null_value = 1;
        return 0;
    }

    // my ugly get-the-job-done 128-bit masking
    memset(result, 0, INET6_ADDRLEN);
    for (i = 0; i < INET6_ADDRLEN, mask >= 8; i++, mask -= 8)
    {
        result[i] = args->args[0][i];
    }
    if (mask)
    {
        result[i] = args->args[0][i++] & (((~0) << (8-mask)) & 255);
    }

    *res_length = length;
    return result;
}

/**
 * inet6_lookup()
 *
 * Resolve host name to IPv6 or IPv4 address in presentation form.
 *
 * Example:
 *   SELECT
 *     INET6_LOOKUP('api.watchmouse.com')),
 *     INET6_LOOKUP('it.ipv6.watchmouse.com'));
 *
 * @arg    string   varchar containing host name
 * @return string   resolved IP in presentation form
 */
my_bool inet6_lookup_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT)
    {
        strcpy(message, "Wrong argument to INET6_LOOKUP: Provide a host name.");
        return 1;
    }
    initid->max_length = INET6_ADDRSTRLEN + 1;
    initid->maybe_null = 1;
    initid->const_item = 0;
    return 0;
}

void inet6_lookup_deinit(UDF_INIT *initid __attribute__((unused)))
{
}

char *inet6_lookup(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args, char *result, unsigned long *res_length,
        char *null_value, char *error __attribute__((unused)))
{
    struct addrinfo *info;
    char *addr;
    ushort i;

    if (!args->args[0] || !args->lengths[0])
    {
        *null_value = 1;
        return 0;
    }

    if (getaddrinfo(args->args[0], NULL, NULL, &info) != 0)
    {
        *null_value = 1;
        return 0;
    }

    // assume first address in list is random
    if (info->ai_family == AF_INET6)
    {
        addr = (char *) &((struct sockaddr_in6 *) info->ai_addr)->sin6_addr.s6_addr;
    }
    else if (info->ai_family == AF_INET)
    {
        addr = (char *) &((struct sockaddr_in *) info->ai_addr)->sin_addr.s_addr;
    }
    else
    {
        *null_value = 1;
        return 0;
    }

    // convert
    if (!inet_ntop(info->ai_family, addr, result, INET6_ADDRSTRLEN + 1))
    {
        *null_value = 1;
        return 0;
    }

    *res_length = strlen(result);
    freeaddrinfo(info);

    return result;
}

/**
 * inet6_rlookup()
 *
 * Resolve IPv6 or IPv4 address in binary or presentation form to host name.
 *
 * Example:
 *   SELECT
 *     INET6_RLOOKUP('2001:4860:a005::68'),
 *     INET6_RLOOKUP('64.128.190.61'),
 *     INET6_RLOOKUP(INET6_PTON('2001:4860:a005::68')),
 *     INET6_RLOOKUP(INET6_PTON('64.128.190.61'));
 *
 * @arg    string   varchar or varbinary format ipv4 or ipv6 address
 * @return string   resolved host in presentation form
 */
my_bool inet6_rlookup_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    if (args->arg_count != 1 || args->arg_type[0] != STRING_RESULT)
    {
        strcpy(message, "Wrong arguments to INET6_RLOOKUP: Provide IPv4 or IPv6 address.");
        return 1;
    }
    initid->max_length = NI_MAXHOST;
    initid->maybe_null = 1;
    initid->const_item = 0;
    return 0;
}

void inet6_rlookup_deinit(UDF_INIT *initid __attribute__((unused)))
{
}

char *inet6_rlookup(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args, char *result, unsigned long *res_length,
        char *null_value, char *error __attribute__((unused)))
{
    const struct sockaddr_storage sa;
    const char *addr = args->args[0], *ptr;
    uint length = args->lengths[0];
    char temp[INET6_ADDRLEN];
    ushort i;

    if (!addr || !length)
    {
        *null_value = 1;
        return 0;
    }

    // looks like presentation string? try to convert
    if ((ptr = strpbrk(addr, ".:")) && strpbrk(ptr, ".:"))
    {
        char temp2[INET6_ADDRSTRLEN + 1];
        int af;

        // cannot assume null-terminated string according to manual
        if (length >= sizeof(temp2))
            length = sizeof(temp2) - 1;
        memcpy(temp2, addr, length);
        temp2[length] = 0;

        // address family
        if (strpbrk(temp2, ":"))
        {
            af = AF_INET6;
            length = INET6_ADDRLEN;
        }
        else
        {
            af = AF_INET;
            length = INET_ADDRLEN;
        }

        // convert
        if (inet_pton(af, temp2, temp) != 1)
        {
            // failed? just use original string
            length = args->lengths[0];
            memcpy(temp, addr, length);
        }
    }
    else
    {
        length = args->lengths[0];
        memcpy(temp, addr, length);
    }

    // now we have temp in binary format

    if (length == INET6_ADDRLEN)
    {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *) &sa;

        sa6->sin6_family = AF_INET6;
        for (i = 0; i < length; i++)
            sa6->sin6_addr.s6_addr[i] = temp[i];

    }
    else if (length == INET_ADDRLEN)
    {
        struct sockaddr_in *sa4 = (struct sockaddr_in *) &sa;
        char *dest = (char *) &sa4->sin_addr.s_addr;

        sa4->sin_family = AF_INET;
        for (i = 0; i < length; i++)
            dest[i] = temp[i];

    }
    else
    {
        *null_value = 1;
        return 0;
    }

    if (getnameinfo((struct sockaddr *) &sa, sizeof(sa), result, NI_MAXHOST, NULL, 0, NI_NAMEREQD))
    {
        *null_value = 1;
        return 0;
    }

    *res_length = strlen(result);
    return result;
}
