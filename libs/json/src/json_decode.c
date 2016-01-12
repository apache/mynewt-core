/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


#include <stdio.h>

#include <json/json.h>

/** 
 * This file is based upon microjson, from Eric S Raymond. 
 *
 * License information for MicroJSON is in the package file MSJSON_COPYING, 
 * it is BSD licensed source code. 
 */


/****************************************************************************

NAME
   mjson.c - parse JSON into fixed-extent data structures

DESCRIPTION
   This module parses a large subset of JSON (JavaScript Object
Notation).  Unlike more general JSON parsers, it doesn't use malloc(3)
and doesn't support polymorphism; you need to give it a set of
template structures describing the expected shape of the incoming
JSON, and it will error out if that shape is not matched.  When the
parse succeeds, attribute values will be extracted into static
locations specified in the template structures.

   The "shape" of a JSON object in the type signature of its
attributes (and attribute values, and so on recursively down through
all nestings of objects and arrays).  This parser is indifferent to
the order of attributes at any level, but you have to tell it in
advance what the type of each attribute value will be and where the
parsed value will be stored. The template structures may supply
default values to be used when an expected attribute is omitted.

   The preceding paragraph told one fib.  A single attribute may
actually have a span of multiple specifications with different
syntactically distinguishable types (e.g. string vs. real vs. integer
vs. boolean, but not signed integer vs. unsigned integer).  The parser
will match the right spec against the actual data.

   The dialect this parses has some limitations.  First, it cannot
recognize the JSON "null" value. Second, all elements of an array must
be of the same type. Third, characters may not be array elements (this
restriction could be lifted)

   There are separate entry points for beginning a parse of either
JSON object or a JSON array. JSON "float" quantities are actually
stored as doubles.

   This parser processes object arrays in one of two different ways,
defending on whether the array subtype is declared as object or
structobject.

   Object arrays take one base address per object subfield, and are
mapped into parallel C arrays (one per subfield).  Strings are not
supported in this kind of array, as they don't have a "natural" size
to use as an offset multiplier.

   Structobjects arrays are a way to parse a list of objects to a set
of modifications to a corresponding array of C structs.  The trick is
that the array object initialization has to specify both the C struct
array's base address and the stride length (the size of the C struct).
If you initialize the offset fields with the correct offsetof calls,
everything will work. Strings are supported but all string storage
has to be inline in the struct.

PERMISSIONS
   This file is Copyright (c) 2014 by Eric S. Raymond
   BSD terms apply: see the file COPYING in the distribution root for details.

***************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>        /* for HUGE_VAL */

static char *
json_target_address(const struct json_attr_t *cursor, 
        const struct json_array_t *parent, int offset)
{
    char *targetaddr = NULL;
    if (parent == NULL || parent->element_type != t_structobject) {
        /* ordinary case - use the address in the cursor structure */
        switch (cursor->type) {
        case t_ignore:
            targetaddr = NULL;
            break;
        case t_integer:
            targetaddr = (char *)&cursor->addr.integer[offset];
            break;
        case t_uinteger:
            targetaddr = (char *)&cursor->addr.uinteger[offset];
            break;
        case t_real:
            targetaddr = (char *)&cursor->addr.real[offset];
            break;
        case t_string:
            targetaddr = cursor->addr.string;
            break;
        case t_boolean:
            targetaddr = (char *)&cursor->addr.boolean[offset];
            break;
        case t_character:
            targetaddr = (char *)&cursor->addr.character[offset];
            break;
        default:
            targetaddr = NULL;
            break;
        }
    } else {
        /* tricky case - hacking a member in an array of structures */
        targetaddr =
            parent->arr.objects.base + (offset * parent->arr.objects.stride) +
            cursor->addr.offset;
    }
    return targetaddr;
}

static int 
json_internal_read_object(const char *cp, const struct json_attr_t *attrs, 
        const struct json_array_t *parent, int offset, const char **end)
{
    enum { 
        init, await_attr, in_attr, await_value, in_val_string,
        in_escape, in_val_token, post_val, post_array
    } state = 0;
    char attrbuf[JSON_ATTR_MAX + 1], *pattr = NULL;
    char valbuf[JSON_VAL_MAX + 1], *pval = NULL;
    bool value_quoted = false;
    char uescape[5];                /* enough space for 4 hex digits and a NUL */
    const struct json_attr_t *cursor;
    int substatus, n, maxlen = 0;
    unsigned int u;
    const struct json_enum_t *mp;
    char *lptr;

#ifdef S_SPLINT_S
    /* prevents gripes about buffers not being completely defined */
    memset(valbuf, '\0', sizeof(valbuf));
    memset(attrbuf, '\0', sizeof(attrbuf));
#endif /* S_SPLINT_S */

    if (end != NULL) { 
        /* give it a well-defined value on parse failure */
        *end = NULL; 
    }

    /* stuff fields with defaults in case they're omitted in the JSON input */
    for (cursor = attrs; cursor->attribute != NULL; cursor++) {
        if (!cursor->nodefault) {
            lptr = json_target_address(cursor, parent, offset);
            if (lptr != NULL)
                switch (cursor->type) {
                case t_integer:
                    memcpy(lptr, &cursor->dflt.integer, sizeof(int));
                    break;
                case t_uinteger:
                    memcpy(lptr, &cursor->dflt.uinteger, sizeof(unsigned int));
                    break;
                case t_real:
                    memcpy(lptr, &cursor->dflt.real, sizeof(double));
                    break;
                case t_string:
                    if (parent != NULL
                        && parent->element_type != t_structobject
                        && offset > 0) {
                        return JSON_ERR_NOPARSTR;
                    }
                    lptr[0] = '\0';
                    break;
                case t_boolean:
                    memcpy(lptr, &cursor->dflt.boolean, sizeof(bool));
                    break;
                case t_character:
                    lptr[0] = cursor->dflt.character;
                    break;
                case t_object:        /* silences a compiler warning */
                case t_structobject:
                case t_array:
                case t_check:
                case t_ignore:
                    break;
                }
        }
    }

    /* parse input JSON */
    for (; *cp != '\0'; cp++) {
        switch (state) {
        case init:
            if (isspace((unsigned char) *cp)) {
                continue;
            } else if (*cp == '{') {
                state = await_attr;
            } else {
                if (end != NULL) {
                    *end = cp;
                }
                return JSON_ERR_OBSTART;
            }
            break;
        case await_attr:
            if (isspace((unsigned char) *cp)) {
                continue;
            } else if (*cp == '"') {
                state = in_attr;
                pattr = attrbuf;
                if (end != NULL) {
                    *end = cp;
                }
            } else if (*cp == '}') {
                break;
            } else {
                if (end != NULL) {
                    *end = cp;
                }
                return JSON_ERR_ATTRSTART;
            }
            break;
        case in_attr:
            if (pattr == NULL) {
                /* don't update end here, leave at attribute start */
                return JSON_ERR_NULLPTR;
            }
            if (*cp == '"') {
                *pattr++ = '\0';
                for (cursor = attrs; cursor->attribute != NULL; cursor++) {
                    if (strcmp(cursor->attribute, attrbuf) == 0) {
                        break;
                    }
                }
                if (cursor->attribute == NULL) {
                    /* don't update end here, leave at attribute start */
                    return JSON_ERR_BADATTR;
                }
                state = await_value;
                if (cursor->type == t_string) {
                    maxlen = (int)cursor->len - 1;
                } else if (cursor->type == t_check) {
                    maxlen = (int)strlen(cursor->dflt.check);
                } else if (cursor->type == t_ignore) {
                    maxlen = JSON_VAL_MAX;
                } else if (cursor->map != NULL) {
                    maxlen = (int)sizeof(valbuf) - 1;
                }
                pval = valbuf;
            } else if (pattr >= attrbuf + JSON_ATTR_MAX - 1) {
                /* don't update end here, leave at attribute start */
                return JSON_ERR_ATTRLEN;
            } else {
                *pattr++ = *cp;
            }
            break;
        case await_value:
            if (isspace((unsigned char) *cp) || *cp == ':') {
                continue;
            } else if (*cp == '[') {
                if (cursor->type != t_array) {
                    if (end != NULL) {
                        *end = cp;
                    }
                    return JSON_ERR_NOARRAY;
                }
                substatus = json_read_array(cp, &cursor->addr.array, &cp);
                if (substatus != 0) {
                    return substatus;
                }
                state = post_array;
            } else if (cursor->type == t_array) {
                if (end != NULL) {
                    *end = cp;
                }
                return JSON_ERR_NOBRAK;
            } else if (*cp == '"') {
                value_quoted = true;
                state = in_val_string;
                pval = valbuf;
            } else {
                value_quoted = false;
                state = in_val_token;
                pval = valbuf;
                *pval++ = *cp;
            }
            break;
        case in_val_string:
            if (pval == NULL) {
                /* don't update end here, leave at value start */
                return JSON_ERR_NULLPTR;
            }
            if (*cp == '\\') {
                state = in_escape;
            } else if (*cp == '"') {
                *pval++ = '\0';
                state = post_val;
            } else if (pval > valbuf + JSON_VAL_MAX - 1
                       || pval > valbuf + maxlen) {
                /* don't update end here, leave at value start */
                return JSON_ERR_STRLONG;        /*  */
            } else {
                *pval++ = *cp;
            }
            break;
        case in_escape:
            if (pval == NULL) {
                /* don't update end here, leave at value start */
                return JSON_ERR_NULLPTR;
            }
            switch (*cp) {
            case 'b':
                *pval++ = '\b';
                break;
            case 'f':
                *pval++ = '\f';
                break;
            case 'n':
                *pval++ = '\n';
                break;
            case 'r':
                *pval++ = '\r';
                break;
            case 't':
                *pval++ = '\t';
                break;
            case 'u':
                for (n = 0; n < 4 && cp[n] != '\0'; n++) {
                    uescape[n] = *cp++;
                }
                --cp;
                (void)sscanf(uescape, "%04x", &u);
                *pval++ = (char)u;        /* will truncate values above 0xff */
                break;
            default:                /* handles double quote and solidus */
                *pval++ = *cp;
                break;
            }
            state = in_val_string;
            break;
        case in_val_token:
            if (pval == NULL) {
                /* don't update end here, leave at value start */
                return JSON_ERR_NULLPTR;
            }
            if (isspace((unsigned char) *cp) || *cp == ',' || *cp == '}') {
                *pval = '\0';
                state = post_val;
                if (*cp == '}' || *cp == ',') {
                    --cp;
                }
            } else if (pval > valbuf + JSON_VAL_MAX - 1) {
                /* don't update end here, leave at value start */
                return JSON_ERR_TOKLONG;
            } else {
                *pval++ = *cp;
            }
            break;
        case post_val:
            /*
             * We know that cursor points at the first spec matching
             * the current attribute.  We don't know that it's *the*
             * correct spec; our dialect allows there to be any number
             * of adjacent ones with the same attrname but different
             * types.  Here's where we try to seek forward for a
             * matching type/attr pair if we're not looking at one.
             */
            for (;;) {
                int seeking = cursor->type;
                if (value_quoted && (cursor->type == t_string)) {
                    break;
                }
                if ((strcmp(valbuf, "true")==0 || strcmp(valbuf, "false")==0)
                        && seeking == t_boolean) {
                    break;
                }
                if (isdigit((unsigned char) valbuf[0])) {
                    bool decimal = strchr(valbuf, '.') != NULL;
                    if (decimal && seeking == t_real) {
                        break;
                    }
                    if (!decimal && (seeking == t_integer || seeking == t_uinteger)) {
                        break;
                    }
                }
                if (cursor[1].attribute==NULL) {       /* out of possiblities */
                    break;
                }
                if (strcmp(cursor[1].attribute, attrbuf)!=0) {
                    break;
                }
                ++cursor;
            }
            if (value_quoted
                && (cursor->type != t_string && cursor->type != t_character
                    && cursor->type != t_check && cursor->type != t_ignore 
                    && cursor->map == 0)) {
                return JSON_ERR_QNONSTRING;
            }
            if (!value_quoted
                && (cursor->type == t_string || cursor->type == t_check
                    || cursor->map != 0)) {
                return JSON_ERR_NONQSTRING;
            }
            if (cursor->map != 0) {
                for (mp = cursor->map; mp->name != NULL; mp++) {
                    if (strcmp(mp->name, valbuf) == 0) {
                        goto foundit;
                    }
                }
                return JSON_ERR_BADENUM;
              foundit:
                (void)snprintf(valbuf, sizeof(valbuf), "%d", mp->value);
            }
            lptr = json_target_address(cursor, parent, offset);
            if (lptr != NULL) {
                switch (cursor->type) {
                case t_integer: {
                        int tmp = atoi(valbuf);
                        memcpy(lptr, &tmp, sizeof(int));
                    }
                    break;
                case t_uinteger: {
                        unsigned int tmp = (unsigned int)atoi(valbuf);
                        memcpy(lptr, &tmp, sizeof(unsigned int));
                    }
                    break;
                case t_real: {
                        double tmp = atof(valbuf);
                        memcpy(lptr, &tmp, sizeof(double));
                    }
                    break;
                case t_string:
                    if (parent != NULL
                        && parent->element_type != t_structobject
                        && offset > 0) {
                        return JSON_ERR_NOPARSTR;
                    }
                    (void)strncpy(lptr, valbuf, cursor->len);
                    valbuf[sizeof(valbuf)-1] = '\0';
                    break;
                case t_boolean: {
                        bool tmp = (strcmp(valbuf, "true") == 0);
                        memcpy(lptr, &tmp, sizeof(bool));
                    }
                    break;
                case t_character:
                    if (strlen(valbuf) > 1) {
                        /* don't update end here, leave at value start */
                        return JSON_ERR_STRLONG;
                    } else {
                        lptr[0] = valbuf[0];
                    }
                    break;
                case t_ignore:        /* silences a compiler warning */
                case t_object:        /* silences a compiler warning */
                case t_structobject:
                case t_array:
                    break;
                case t_check:
                    if (strcmp(cursor->dflt.check, valbuf) != 0) {
                        /* don't update end here, leave at start of attribute */
                        return JSON_ERR_CHECKFAIL;
                    }
                    break;
                }
            }
            /*@fallthrough@*/
        case post_array:
            if (isspace((unsigned char) *cp)) {
                continue;
            } else if (*cp == ',') {
                state = await_attr;
            } else if (*cp == '}') {
                ++cp;
                goto good_parse;
            } else {
                if (end != NULL) {
                    *end = cp;
                }
                return JSON_ERR_BADTRAIL;
            }
            break;
        }
    }

  good_parse:
    /* in case there's another object following, consume trailing WS */
    while (isspace((unsigned char) *cp)) {
        ++cp;
    }
    if (end != NULL) {
        *end = cp;
    }
    return 0;
}

int 
json_read_array(const char *cp, const struct json_array_t *arr, 
        const char **end)
{
    int substatus, offset, arrcount;
    char *tp;

    if (end != NULL) {
        /* give it a well-defined value on parse failure */
        *end = NULL;
    }

    while (isspace((unsigned char) *cp)) {
        cp++;
    }

    if (*cp != '[') {
        return JSON_ERR_ARRAYSTART;
    } else {
        cp++;
    }

    tp = arr->arr.strings.store;
    arrcount = 0;

    /* Check for empty array */
    while (isspace((unsigned char) *cp)) {
        cp++;
    }
    if (*cp == ']') {
        goto breakout;
    }

    for (offset = 0; offset < arr->maxlen; offset++) {
        char *ep = NULL;
        switch (arr->element_type) {
        case t_string:
            if (isspace((unsigned char) *cp)) {
                cp++;
            }
            if (*cp != '"') {
                return JSON_ERR_BADSTRING;
            } else {
                ++cp;
            }
            arr->arr.strings.ptrs[offset] = tp;
            for (; tp - arr->arr.strings.store < arr->arr.strings.storelen;
                 tp++) {
                if (*cp == '"') {
                    ++cp;
                    *tp++ = '\0';
                    goto stringend;
                } else if (*cp == '\0') {
                    return JSON_ERR_BADSTRING;
                } else {
                    *tp = *cp++;
                }
            }
            return JSON_ERR_BADSTRING;
          stringend:
            break;
        case t_object:
        case t_structobject:
            substatus =
                json_internal_read_object(cp, arr->arr.objects.subtype, arr,
                                          offset, &cp);
            if (substatus != 0) {
                if (end != NULL) {
                    end = &cp;
                }
                return substatus;
            }
            break;
        case t_integer:
            arr->arr.integers.store[offset] = (int)strtol(cp, &ep, 0);
            if (ep == cp) {
                return JSON_ERR_BADNUM;
            } else {
                cp = ep;
            }
            break;
        case t_uinteger:
            arr->arr.uintegers.store[offset] = (unsigned int)strtoul(cp, &ep, 0);
            if (ep == cp) {
                return JSON_ERR_BADNUM;
            } else {
                cp = ep;
            }
            break;
        case t_real:
            arr->arr.reals.store[offset] = strtod(cp, &ep);
            if (ep == cp) {
                return JSON_ERR_BADNUM;
            } else {
                cp = ep;
            }
            break;
        case t_boolean:
            if (strncmp(cp, "true", 4) == 0) {
                arr->arr.booleans.store[offset] = true;
                cp += 4;
            }
            else if (strncmp(cp, "false", 5) == 0) {
                arr->arr.booleans.store[offset] = false;
                cp += 5;
            }
            break;
        case t_character:
        case t_array:
        case t_check:
        case t_ignore:
            return JSON_ERR_SUBTYPE;
        }
        arrcount++;
        if (isspace((unsigned char) *cp)) {
            cp++;
        }
        if (*cp == ']') {
            goto breakout;
        } else if (*cp == ',') {
            cp++;
        } else {
            return JSON_ERR_BADSUBTRAIL;
        }
    }
    if (end != NULL) {
        *end = cp;
    }
    return JSON_ERR_SUBTOOLONG;
  breakout:
    if (arr->count != NULL) {
        *(arr->count) = arrcount;
    }
    if (end != NULL) {
        *end = cp;
    }
    return 0;
}

int 
json_read_object(const char *cp, const struct json_attr_t *attrs, 
        const char **end)
{
    int st;

    st = json_internal_read_object(cp, attrs, NULL, 0, end);
    return st;
}

