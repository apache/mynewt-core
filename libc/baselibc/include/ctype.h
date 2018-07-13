/*
 * ctype.h
 *
 * This assumes ASCII.
 */

#ifndef _CTYPE_H
#define _CTYPE_H

#include <klibc/extern.h>
#include <klibc/inline.h>

#ifdef __cplusplus
extern "C" {
#endif

__extern_inline int isupper(int __c)
{
	return __c >= 'A' && __c <= 'Z';
}

__extern_inline int islower(int __c)
{
	return __c >= 'a' && __c <= 'z';
}

__extern_inline int isalpha(int __c)
{
	return islower(__c) || isupper(__c);
}

__extern_inline int isdigit(int __c)
{
	return ((unsigned)__c - '0') <= 9;
}

__extern_inline int isalnum(int __c)
{
	return isalpha(__c) || isdigit(__c);
}

__extern_inline int isascii(int __c)
{
	return !(__c & ~0x7f);
}

__extern_inline int isblank(int __c)
{
	return (__c == '\t') || (__c == ' ');
}

__extern_inline int iscntrl(int __c)
{
	return __c < 0x20;
}

__extern_inline int isspace(int __c)
{
	return __c == ' ' || __c == '\n' || __c == '\t' || __c == '\r';
}

__extern_inline int isxdigit(int __c)
{
	return isdigit(__c) || (__c >= 'a' && __c <= 'f') || (__c >= 'A' && __c <= 'F');
}

__extern_inline int ispunct(int __c)
{
	return (__c >= '!' && __c <= '/') ||
	    (__c >= ':' && __c <= '@') ||
	    (__c >= '[' && __c <= '`') ||
	    (__c >= '{' && __c <= '~');
}

__extern_inline int isprint(int __c)
{
	return (__c >= 0x20 && __c <= 0x7e);
}

__extern_inline int isgraph(int __c)
{
	return (__c > 0x20 && __c < 0x7f);
}

__extern_inline int toupper(int __c)
{
	return islower(__c) ? (__c & ~32) : __c;
}

__extern_inline int tolower(int __c)
{
	return isupper(__c) ? (__c | 32) : __c;
}

#ifdef __cplusplus
}
#endif

#endif				/* _CTYPE_H */
