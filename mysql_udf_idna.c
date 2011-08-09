/**
 * mysql_udf_idna.c
 *
 * Trivial MySQL functions for internationalised (idna) domain names.
 *
 * author: Pieter Ennes
 * email:  labs@watchmouse.com
 * web:    http://labs.watchmouse.com/mysql-udf-idna/
 *
 * Copyright (c) 2011 WatchMouse
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
 * @TODO: Optimise memory use by using lower-level idna functions
 * @TODO: User default character set from MySQL
 */

#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include <idna.h>
#include <stringprep.h>

#include <mysql/mysql.h>

#define min(x, y)       ((x) < (y) ? (x) : (y))
#define max(x, y)       ((x) > (y) ? (x) : (y))

// via limits.h, should be 255 at least to be POSIX compliant
// note that the maximum *domain* name length is still 63 characters
#ifdef _POSIX_HOST_NAME_MAX
#ifdef HOST_NAME_MAX
#define MAX_HOSTNAME_LEN max(HOST_NAME_MAX, _POSIX_HOST_NAME_MAX)
#else
#define MAX_HOSTNAME_LEN _POSIX_HOST_NAME_MAX
#endif
#else
#define MAX_HOSTNAME_LEN 255
#endif

// default encoding to use when charset argument is omitted
#define DEFAULT_CHARSET "UTF-8"

my_bool idna_from_ascii_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void idna_from_ascii_deinit(UDF_INIT *initid);
char *idna_from_ascii(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length,
        char *null_value, char *error);

my_bool idna_to_ascii_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void idna_to_ascii_deinit(UDF_INIT *initid);
char *idna_to_ascii(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length,
        char *null_value, char *error);

my_bool idna_from_ascii2_init(UDF_INIT *initid, UDF_ARGS *args, char *message);
void idna_from_ascii2_deinit(UDF_INIT *initid);
char *idna_from_ascii2(UDF_INIT *initid, UDF_ARGS *args, char *result, unsigned long *length,
        char *null_value, char *error);

/**
 * idna_from_ascii()
 *
 * This function decodes a possible ASCII compatible representation of the provided
 * host or domain name. The result is converted into the character set provided in
 * the second argument.
 *
 * @arg    string   a valid host or domain name
 * @arg    string   a valid character set name, default is DEFAULT_CHARSET
 * @return string   ASCII compatible representation of the provided host name
 */
my_bool idna_from_ascii_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    if (args->arg_count < 1 || args->arg_type[0] != STRING_RESULT)
    {
        strcpy(message, "provide punycoded domain name");
        return 1;
    }
    if (args->arg_count > 1 && args->arg_type[1] != STRING_RESULT)
    {
        strcpy(message, "provide valid character set");
        return 1;
    }
    initid->max_length = MAX_HOSTNAME_LEN;
    initid->maybe_null = 1;
    initid->const_item = 0;
    return 0;
}

void idna_from_ascii_deinit(UDF_INIT *initid __attribute__((unused)))
{
}


char *idna_from_ascii(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args, char *result, unsigned long *res_length,
        char *null_value, char *error __attribute__((unused)))
{
    char temp[MAX_HOSTNAME_LEN+1];
    char *temp2;
    char *charset = NULL;
    uint length;

    if (!args->args[0] || !(length = args->lengths[0]))
    {
        *null_value = 1;
        return 0;
    }

    if (args->arg_count > 1 && args->args[1] && args->lengths[1])
    {
    	charset = args->args[1];
    }

    // cannot assume null-terminated string according to manual
    if (length >= sizeof(temp))
        length = sizeof(temp) - 1;
    memcpy(temp, args->args[0], length);
    temp[length] = 0;

    // convert to utf8
    if (idna_to_unicode_8z8z(temp, &temp2, 0) != IDNA_SUCCESS)
    {
        *null_value = 1;
        return 0;
    }

    // convert from utf8 to user encoding
    if (charset)
    {
    	char *temp3 = temp2;
    	temp2 = stringprep_convert(temp2, DEFAULT_CHARSET, charset);
    	free(temp3);

    	if (!temp2)
        {
            *null_value = 1;
            return 0;
        }
    }

    strncpy(result, temp2, initid->max_length);

    free(temp2);

    *res_length = strlen(result);

    return result;
}

/**
 * idna_to_ascii()
 *
 * This function returns an ASCII compatible representation of the provided (internationalised) host or
 * domain name, which should be encoded as indicated by the provided character set.
 *
 * @arg    string   a valid host or domain name
 * @arg    string   a valid character set name, default is DEFAULT_CHARSET
 * @return string   ASCII compatible representation of the provided host name
 */
my_bool idna_to_ascii_init(UDF_INIT *initid, UDF_ARGS *args, char *message)
{
    if (args->arg_count < 1 || args->arg_type[0] != STRING_RESULT)
    {
        strcpy(message, "provide domain name to punycode");
        return 1;
    }
    if (args->arg_count > 1 && args->arg_type[1] != STRING_RESULT)
    {
        strcpy(message, "provide valid character set");
        return 1;
    }
    initid->max_length = MAX_HOSTNAME_LEN;
    initid->maybe_null = 1;
    initid->const_item = 0;
    return 0;
}

void idna_to_ascii_deinit(UDF_INIT *initid __attribute__((unused)))
{
}

char *idna_to_ascii(UDF_INIT *initid __attribute__((unused)), UDF_ARGS *args, char *result, unsigned long *res_length,
        char *null_value, char *error __attribute__((unused)))
{
    char temp[MAX_HOSTNAME_LEN+1];
    char *temp2, *temp3;
    char *charset = NULL;
    uint length;

    if (!args->args[0] || !(length = args->lengths[0]))
    {
        *null_value = 1;
        return 0;
    }

    if (args->arg_count > 1 && args->args[1] && args->lengths[1])
    {
    	charset = args->args[1];
    }

    // cannot assume null-terminated string according to manual
    if (length >= sizeof(temp))
        length = sizeof(temp) - 1;
    memcpy(temp, args->args[0], length);
    temp[length] = 0;

    // convert from user encoding to utf8
    if (charset)
    {
    	temp2 = stringprep_convert(temp, charset, DEFAULT_CHARSET);

    	if (!temp2)
        {
            *null_value = 1;
            return 0;
        }
    }
    else
    {
    	temp2 = temp;
    }

    // convert to utf8
    if (idna_to_ascii_8z(temp2, &temp3, 0) != IDNA_SUCCESS)
    {
    	if (charset)
    		free(temp2);
        *null_value = 1;
        return 0;
    }

    strncpy(result, temp3, initid->max_length);

    if (charset)
    	free(temp2);

    free(temp3);

    *res_length = strlen(result);

    return result;
}
