#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "json/json.h"

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
#include <math.h>        /* for HUGE_VAL */

static void
json_skip_ws(struct json_buffer *jb)
{
    char c;

    do {
        c = jb->jb_read_next(jb);
    } while (isspace((int) c));

    jb->jb_read_prev(jb);
}

static char
json_peek(struct json_buffer *jb)
{
    char c;

    jb->jb_read_next(jb);
    c = jb->jb_read_prev(jb);

    return c;
}

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
json_internal_read_object(struct json_buffer *jb,
                          const struct json_attr_t *attrs,
                          const struct json_array_t *parent,
                          int offset)
{
    char c;
    enum {
        init, await_attr, in_attr, await_value, in_val_string,
        in_escape, in_val_token, post_val, post_array
    } state = 0;
    char attrbuf[JSON_ATTR_MAX + 1], *pattr = NULL;
    char valbuf[JSON_VAL_MAX + 1], *pval = NULL;
    bool value_quoted = false;
    char uescape[5];    /* enough space for 4 hex digits and '\0' */
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

    /* stuff fields with defaults in case they're omitted in the JSON input */
    for (cursor = attrs; cursor->attribute != NULL; cursor++) {
        if (!cursor->nodefault) {
            lptr = json_target_address(cursor, parent, offset);
            if (lptr != NULL) {
                switch (cursor->type) {
                case t_integer:
                    memcpy(lptr, &cursor->dflt.integer, sizeof(long long int));
                    break;
                case t_uinteger:
                    memcpy(lptr, &cursor->dflt.uinteger,
                           sizeof(long long unsigned int));
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
    }

    /* parse input JSON */
    for (c = jb->jb_read_next(jb); c != '\0'; c = jb->jb_read_next(jb)) {
        switch (state) {
        case init:
            if (isspace((unsigned char) c)) {
                continue;
            } else if (c == '{') {
                state = await_attr;
            } else {
                return JSON_ERR_OBSTART;
            }
            break;
        case await_attr:
            if (isspace((unsigned char) c)) {
                continue;
            } else if (c == '"') {
                state = in_attr;
                pattr = attrbuf;
            } else if (c == '}') {
                break;
            } else {
                return JSON_ERR_ATTRSTART;
            }
            break;
        case in_attr:
            if (pattr == NULL) {
                /* don't update end here, leave at attribute start */
                return JSON_ERR_NULLPTR;
            }
            if (c == '"') {
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
                } else if (cursor->type == t_boolean) {
                    maxlen = 5; /* false */
                }
                pval = valbuf;
            } else if (pattr >= attrbuf + JSON_ATTR_MAX - 1) {
                /* don't update end here, leave at attribute start */
                return JSON_ERR_ATTRLEN;
            } else {
                *pattr++ = c;
            }
            break;
        case await_value:
            if (isspace((unsigned char) c) || c == ':') {
                continue;
            } else if (c == '[') {
                if (cursor->type != t_array) {
                    return JSON_ERR_NOARRAY;
                }
                c = jb->jb_read_prev(jb);
                substatus = json_read_array(jb, &cursor->addr.array);
                if (substatus != 0) {
                    return substatus;
                }
                state = post_array;
            } else if (cursor->type == t_array) {
                return JSON_ERR_NOBRAK;
            } else if (c == '"') {
                value_quoted = true;
                state = in_val_string;
                pval = valbuf;
            } else {
                value_quoted = false;
                state = in_val_token;
                pval = valbuf;
                *pval++ = c;
            }
            break;
        case in_val_string:
            if (pval == NULL) {
                /* don't update end here, leave at value start */
                return JSON_ERR_NULLPTR;
            }
            if (c == '\\') {
                state = in_escape;
            } else if (c == '"') {
                *pval++ = '\0';
                state = post_val;
            } else if (pval > valbuf + JSON_VAL_MAX - 1
                       || pval > valbuf + maxlen) {
                /* don't update end here, leave at value start */
                return JSON_ERR_STRLONG;        /*  */
            } else {
                *pval++ = c;
            }
            break;
        case in_escape:
            if (pval == NULL) {
                /* don't update end here, leave at value start */
                return JSON_ERR_NULLPTR;
            }
            switch (c) {
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
                for (n = 0; n < 4 && c != '\0'; n++) {
                    uescape[n] = c;
                    c = jb->jb_read_next(jb);
                }
                // Scroll back one
                c = jb->jb_read_prev(jb);
                u = strtoul(uescape, NULL, 16);
                *pval++ = (char)u;        /* will truncate values above 0xff */
                break;
            default:                /* handles double quote and solidus */
                *pval++ = c;
                break;
            }
            state = in_val_string;
            break;
        case in_val_token:
            if (pval == NULL) {
                /* don't update end here, leave at value start */
                return JSON_ERR_NULLPTR;
            }
            if (isspace((unsigned char) c) || c == ',' || c == '}') {
                *pval = '\0';
                state = post_val;
                if (c == '}' || c == ',') {
                    c = jb->jb_read_prev(jb);
                }
            } else if (pval > valbuf + JSON_VAL_MAX - 1) {
                /* don't update end here, leave at value start */
                return JSON_ERR_TOKLONG;
            } else {
                *pval++ = c;
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
                    if (!decimal && (seeking == t_integer ||
                                     seeking == t_uinteger)) {
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
                (void)snprintf(valbuf, sizeof(valbuf), "%lld", mp->value);
            }
            lptr = json_target_address(cursor, parent, offset);
            if (lptr != NULL) {
                switch (cursor->type) {
                case t_integer: {
                        long long int tmp =
                            (long long int)strtoll(valbuf, NULL, 10);
                        memcpy(lptr, &tmp, sizeof(long long int));
                    }
                    break;
                case t_uinteger: {
                        long long unsigned int tmp =
                            (long long unsigned int)strtoull(valbuf, NULL, 10);
                        memcpy(lptr, &tmp, sizeof(long long unsigned int));
                    }
                    break;
                case t_real: {
#ifdef FLOAT_SUPPORT
                        double tmp = atof(valbuf);
                        memcpy(lptr, &tmp, sizeof(double));
#else
                        return JSON_ERR_MISC;
#endif
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
            if (isspace((unsigned char) c)) {
                continue;
            } else if (c == ',') {
                state = await_attr;
            } else if (c == '}') {
                return 0;
            } else {
                return JSON_ERR_BADTRAIL;
            }
            break;
        }
    }

  return 0;
}

int
json_read_array(struct json_buffer *jb, const struct json_array_t *arr)
{
    char valbuf[64];
    char c;
    int substatus, offset, arrcount;
    char *tp;
    int n, count;

    json_skip_ws(jb);

    if (jb->jb_read_next(jb) != '[') {
        return JSON_ERR_ARRAYSTART;
    }

    tp = arr->arr.strings.store;
    arrcount = 0;

    json_skip_ws(jb);

    if (json_peek(jb) == ']') {
        goto breakout;
    }

    for (offset = 0; offset < arr->maxlen; offset++) {
        json_skip_ws(jb);

        char *ep = NULL;
        switch (arr->element_type) {
        case t_string:
            if (jb->jb_read_next(jb) != '"') {
                return JSON_ERR_BADSTRING;
            }
            arr->arr.strings.ptrs[offset] = tp;
            for (; tp - arr->arr.strings.store < arr->arr.strings.storelen;
                 tp++) {
                c = jb->jb_read_next(jb);
                if (c == '"') {
                    c = jb->jb_read_next(jb);
                    *tp++ = '\0';
                    goto stringend;
                } else if (c == '\0') {
                    return JSON_ERR_BADSTRING;
                } else {
                    *tp = c;
                    c = jb->jb_read_next(jb);
                }
            }
            return JSON_ERR_BADSTRING;
          stringend:
            break;
        case t_object:
        case t_structobject:
            substatus =
                json_internal_read_object(jb, arr->arr.objects.subtype, arr,
                                          offset);
            if (substatus != 0) {
                return substatus;
            }
            break;
        case t_integer:
            n = jb->jb_readn(jb, valbuf, sizeof(valbuf)-1);
            valbuf[n] = '\0';

            arr->arr.integers.store[offset] = strtoll(valbuf, &ep, 0);
            if (ep == valbuf) {
                return JSON_ERR_BADNUM;
            } else {
                count = n - (ep - valbuf);
                while (count-- > 0) {
                    jb->jb_read_prev(jb);
                }
            }
            break;
        case t_uinteger:
            n = jb->jb_readn(jb, valbuf, sizeof(valbuf)-1);
            valbuf[n] = '\0';

            arr->arr.uintegers.store[offset] = strtoull(valbuf, &ep, 0);
            if (ep == valbuf) {
                return JSON_ERR_BADNUM;
            } else {
                count = n - (ep - valbuf);
                while (count-- > 0) {
                    jb->jb_read_prev(jb);
                }
            }
            break;
        case t_real:
#ifdef FLOAT_SUPPORT
            n = jb->jb_readn(jb, valbuf, sizeof(valbuf)-1);
            valbuf[n] = '\0';

            arr->arr.reals.store[offset] = strtod(valbuf, &ep);
            if (ep == valbuf) {
                return JSON_ERR_BADNUM;
            } else {
                count = ep - valbuf;
                while (count-- > 0) {
                    c = jb->jb_read_next(jb);
                }
            }
#else
            return JSON_ERR_MISC;
#endif
            break;
        case t_boolean:
            n = jb->jb_readn(jb, valbuf, 5);
            valbuf[n] = '\0';

            if (strncmp(valbuf, "true", 4) == 0) {
                arr->arr.booleans.store[offset] = true;
                count = n - 4;
            } else if (strncmp(valbuf, "false", 5) == 0) {
                arr->arr.booleans.store[offset] = false;
                count = n - 5;
            } else {
                return JSON_ERR_MISC;
            }

            assert(count >= 0);
            while (count-- > 0) {
                jb->jb_read_prev(jb);
            }
            break;
        case t_character:
        case t_array:
        case t_check:
        case t_ignore:
            return JSON_ERR_SUBTYPE;
        }
        arrcount++;
        json_skip_ws(jb);

        c = jb->jb_read_next(jb);
        if (c == ']') {
            goto breakout;
        } else if (c != ',') {
            return JSON_ERR_BADSUBTRAIL;
        }
    }
    return JSON_ERR_SUBTOOLONG;
  breakout:
    if (arr->count != NULL) {
        *(arr->count) = arrcount;
    }
    return 0;
}

int
json_read_object(struct json_buffer *jb, const struct json_attr_t *attrs)
{
    int st;

    st = json_internal_read_object(jb, attrs, NULL, 0);
    return st;
}

