/**********************************************************************

  pack.c -

  $Author$
  created at: Thu Feb 10 15:17:05 JST 1994

  Copyright (C) 1993-2007 Yukihiro Matsumoto

**********************************************************************/

#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <sys/types.h>

#include "internal.h"
#include "internal/bits.h"
#include "internal/string.h"
#include "internal/symbol.h"
#include "internal/util.h"

// #include "internal/variable.h"
VALUE rb_ivar_lookup(VALUE obj, ID id, VALUE undef);

#include "builtin.h"

/*
 * It is intentional that the condition for natstr is HAVE_TRUE_LONG_LONG
 * instead of HAVE_LONG_LONG or LONG_LONG.
 * This means q! and Q! means always the standard long long type and
 * causes ArgumentError for platforms which has no long long type,
 * even if the platform has an implementation specific 64bit type.
 * This behavior is consistent with the document of pack/unpack.
 */
#ifdef HAVE_TRUE_LONG_LONG
static const char natstr[] = "sSiIlLqQjJ";
#else
static const char natstr[] = "sSiIlLjJ";
#endif
static const char endstr[] = "sSiIlLqQjJ";

#ifdef HAVE_TRUE_LONG_LONG
/* It is intentional to use long long instead of LONG_LONG. */
# define NATINT_LEN_Q NATINT_LEN(long long, 8)
#else
# define NATINT_LEN_Q 8
#endif

#if SIZEOF_SHORT != 2 || SIZEOF_LONG != 4 || (defined(HAVE_TRUE_LONG_LONG) && SIZEOF_LONG_LONG != 8)
# define NATINT_PACK
#endif

#ifdef DYNAMIC_ENDIAN
/* for universal binary of NEXTSTEP and MacOS X */
/* useless since autoconf 2.63? */
static int
is_bigendian(void)
{
    static int init = 0;
    static int endian_value;
    char *p;

    if (init) return endian_value;
    init = 1;
    p = (char*)&init;
    return endian_value = p[0]?0:1;
}
# define BIGENDIAN_P() (is_bigendian())
#elif defined(WORDS_BIGENDIAN)
# define BIGENDIAN_P() 1
#else
# define BIGENDIAN_P() 0
#endif

#ifdef NATINT_PACK
# define NATINT_LEN(type,len) (natint?(int)sizeof(type):(int)(len))
#else
# define NATINT_LEN(type,len) ((int)sizeof(type))
#endif

typedef union {
    float f;
    uint32_t u;
    char buf[4];
} FLOAT_SWAPPER;
typedef union {
    double d;
    uint64_t u;
    char buf[8];
} DOUBLE_SWAPPER;
#define swapf(x) swap32(x)
#define swapd(x) swap64(x)

#define rb_ntohf(x) (BIGENDIAN_P()?(x):swapf(x))
#define rb_ntohd(x) (BIGENDIAN_P()?(x):swapd(x))
#define rb_htonf(x) (BIGENDIAN_P()?(x):swapf(x))
#define rb_htond(x) (BIGENDIAN_P()?(x):swapd(x))
#define rb_htovf(x) (BIGENDIAN_P()?swapf(x):(x))
#define rb_htovd(x) (BIGENDIAN_P()?swapd(x):(x))
#define rb_vtohf(x) (BIGENDIAN_P()?swapf(x):(x))
#define rb_vtohd(x) (BIGENDIAN_P()?swapd(x):(x))

#define FLOAT_CONVWITH(x)	FLOAT_SWAPPER x;
#define HTONF(x)	((x).u = rb_htonf((x).u))
#define HTOVF(x)	((x).u = rb_htovf((x).u))
#define NTOHF(x)	((x).u = rb_ntohf((x).u))
#define VTOHF(x)	((x).u = rb_vtohf((x).u))

#define DOUBLE_CONVWITH(x)	DOUBLE_SWAPPER x;
#define HTOND(x)	((x).u = rb_htond((x).u))
#define HTOVD(x)	((x).u = rb_htovd((x).u))
#define NTOHD(x)	((x).u = rb_ntohd((x).u))
#define VTOHD(x)	((x).u = rb_vtohd((x).u))

#define MAX_INTEGER_PACK_SIZE 8

static unsigned long utf8_to_uv(const char*,long*);

static ID id_associated;

static void
str_associate(VALUE str, VALUE add)
{
    /* assert(NIL_P(rb_attr_get(str, id_associated))); */
    rb_ivar_set(str, id_associated, add);
}

static VALUE
str_associated(VALUE str)
{
    return rb_ivar_lookup(str, id_associated, Qfalse);
}

static void
unknown_directive(const char *mode, char type, VALUE fmt)
{
    VALUE f;
    char unknown[5];

    if (ISPRINT(type)) {
        unknown[0] = type;
        unknown[1] = '\0';
    }
    else {
        snprintf(unknown, sizeof(unknown), "\\x%.2x", type & 0xff);
    }
    f = rb_str_quote_unprintable(fmt);
    if (f != fmt) {
        fmt = rb_str_subseq(f, 1, RSTRING_LEN(f) - 2);
    }
    rb_warning("unknown %s directive '%s' in '%"PRIsVALUE"'",
               mode, unknown, fmt);
}

static const char b64_table[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static inline int
hex2num(char c)
{
    int n;
    n = ruby_digit36_to_number_table[(unsigned char)c];
    if (16 <= n)
        n = -1;
    return n;
}

#define PACK_LENGTH_ADJUST_SIZE(sz) do {	\
    tmp_len = 0;				\
    if (len > (long)((send-s)/(sz))) {		\
        if (!star) {				\
	    tmp_len = len-(send-s)/(sz);	\
        }					\
	len = (send-s)/(sz);			\
    }						\
} while (0)

#define PACK_ITEM_ADJUST() do { \
    if (tmp_len > 0 && mode == UNPACK_ARRAY) \
	rb_ary_store(ary, RARRAY_LEN(ary)+tmp_len-1, Qnil); \
} while (0)

/* Workaround for Oracle Developer Studio (Oracle Solaris Studio)
 * 12.4/12.5/12.6 C compiler optimization bug
 * with "-xO4" optimization option.
 */
#if defined(__SUNPRO_C) && 0x5130 <= __SUNPRO_C && __SUNPRO_C <= 0x5150
# define AVOID_CC_BUG volatile
#else
# define AVOID_CC_BUG
#endif

/* unpack mode */
#define UNPACK_ARRAY 0
#define UNPACK_BLOCK 1
#define UNPACK_1 2

#define castchar(from) (char)((from) & 0xff)

VALUE zscan_internal_unpack(VALUE str, VALUE fmt, long* parsed_len)
{
    char* init_s;
    int mode = UNPACK_ARRAY;
#define hexdigits ruby_hexdigits
    char *s, *send;
    char *p, *pend;
    VALUE ary;
    char type;
    long len;
    AVOID_CC_BUG long tmp_len;
    int star;
#ifdef NATINT_PACK
    int natint;			/* native integer */
#endif
    int signed_p, integer_size, bigendian_p;
#define UNPACK_PUSH(item) do {\
	VALUE item_val = (item);\
	if ((mode) == UNPACK_BLOCK) {\
	    rb_yield(item_val);\
	}\
	else if ((mode) == UNPACK_ARRAY) {\
	    rb_ary_push(ary, item_val);\
	}\
	else /* if ((mode) == UNPACK_1) { */ {\
	    return item_val; \
	}\
    } while (0)

    StringValue(str);
    StringValue(fmt);
    init_s = s = RSTRING_PTR(str);
    send = s + RSTRING_LEN(str);
    p = RSTRING_PTR(fmt);
    pend = p + RSTRING_LEN(fmt);

    ary = mode == UNPACK_ARRAY ? rb_ary_new() : Qnil;
    while (p < pend) {
	int explicit_endian = 0;
	type = *p++;
#ifdef NATINT_PACK
	natint = 0;
#endif

	if (ISSPACE(type)) continue;
	if (type == '#') {
	    while ((p < pend) && (*p != '\n')) {
		p++;
	    }
	    continue;
	}

	star = 0;
	{
          modifiers:
	    switch (*p) {
	      case '_':
	      case '!':

		if (strchr(natstr, type)) {
#ifdef NATINT_PACK
		    natint = 1;
#endif
		    p++;
		}
		else {
		    rb_raise(rb_eArgError, "'%c' allowed only after types %s", *p, natstr);
		}
		goto modifiers;

	      case '<':
	      case '>':
		if (!strchr(endstr, type)) {
		    rb_raise(rb_eArgError, "'%c' allowed only after types %s", *p, endstr);
		}
		if (explicit_endian) {
		    rb_raise(rb_eRangeError, "Can't use both '<' and '>'");
		}
		explicit_endian = *p++;
		goto modifiers;
	    }
	}

	if (p >= pend)
	    len = 1;
	else if (*p == '*') {
	    star = 1;
	    len = send - s;
	    p++;
	}
	else if (ISDIGIT(*p)) {
	    errno = 0;
	    len = STRTOUL(p, (char**)&p, 10);
	    if (len < 0 || errno) {
		rb_raise(rb_eRangeError, "pack length too big");
	    }
	}
	else {
	    len = (type != '@');
	}

	switch (type) {
	  case '%':
	    rb_raise(rb_eArgError, "%% is not supported");
	    break;

	  case 'A':
	    if (len > send - s) len = send - s;
	    {
		long end = len;
		char *t = s + len - 1;

		while (t >= s) {
		    if (*t != ' ' && *t != '\0') break;
		    t--; len--;
		}
                UNPACK_PUSH(rb_str_new(s, len));
		s += end;
	    }
	    break;

	  case 'Z':
	    {
		char *t = s;

		if (len > send-s) len = send-s;
		while (t < s+len && *t) t++;
                UNPACK_PUSH(rb_str_new(s, t-s));
		if (t < send) t++;
		s = star ? t : s+len;
	    }
	    break;

	  case 'a':
	    if (len > send - s) len = send - s;
            UNPACK_PUSH(rb_str_new(s, len));
	    s += len;
	    break;

	  case 'b':
	    {
		VALUE bitstr;
		char *t;
		int bits;
		long i;

		if (p[-1] == '*' || len > (send - s) * 8)
		    len = (send - s) * 8;
		bits = 0;
		bitstr = rb_usascii_str_new(0, len);
		t = RSTRING_PTR(bitstr);
		for (i=0; i<len; i++) {
		    if (i & 7) bits >>= 1;
		    else bits = (unsigned char)*s++;
		    *t++ = (bits & 1) ? '1' : '0';
		}
		UNPACK_PUSH(bitstr);
	    }
	    break;

	  case 'B':
	    {
		VALUE bitstr;
		char *t;
		int bits;
		long i;

		if (p[-1] == '*' || len > (send - s) * 8)
		    len = (send - s) * 8;
		bits = 0;
		bitstr = rb_usascii_str_new(0, len);
		t = RSTRING_PTR(bitstr);
		for (i=0; i<len; i++) {
		    if (i & 7) bits <<= 1;
		    else bits = (unsigned char)*s++;
		    *t++ = (bits & 128) ? '1' : '0';
		}
		UNPACK_PUSH(bitstr);
	    }
	    break;

	  case 'h':
	    {
		VALUE bitstr;
		char *t;
		int bits;
		long i;

		if (p[-1] == '*' || len > (send - s) * 2)
		    len = (send - s) * 2;
		bits = 0;
		bitstr = rb_usascii_str_new(0, len);
		t = RSTRING_PTR(bitstr);
		for (i=0; i<len; i++) {
		    if (i & 1)
			bits >>= 4;
		    else
			bits = (unsigned char)*s++;
		    *t++ = hexdigits[bits & 15];
		}
		UNPACK_PUSH(bitstr);
	    }
	    break;

	  case 'H':
	    {
		VALUE bitstr;
		char *t;
		int bits;
		long i;

		if (p[-1] == '*' || len > (send - s) * 2)
		    len = (send - s) * 2;
		bits = 0;
		bitstr = rb_usascii_str_new(0, len);
		t = RSTRING_PTR(bitstr);
		for (i=0; i<len; i++) {
		    if (i & 1)
			bits <<= 4;
		    else
			bits = (unsigned char)*s++;
		    *t++ = hexdigits[(bits >> 4) & 15];
		}
		UNPACK_PUSH(bitstr);
	    }
	    break;

	  case 'c':
	    signed_p = 1;
	    integer_size = 1;
	    bigendian_p = BIGENDIAN_P(); /* not effective */
	    goto unpack_integer;

	  case 'C':
	    signed_p = 0;
	    integer_size = 1;
	    bigendian_p = BIGENDIAN_P(); /* not effective */
	    goto unpack_integer;

	  case 's':
	    signed_p = 1;
	    integer_size = NATINT_LEN(short, 2);
	    bigendian_p = BIGENDIAN_P();
	    goto unpack_integer;

	  case 'S':
	    signed_p = 0;
	    integer_size = NATINT_LEN(short, 2);
	    bigendian_p = BIGENDIAN_P();
	    goto unpack_integer;

	  case 'i':
	    signed_p = 1;
	    integer_size = (int)sizeof(int);
	    bigendian_p = BIGENDIAN_P();
	    goto unpack_integer;

	  case 'I':
	    signed_p = 0;
	    integer_size = (int)sizeof(int);
	    bigendian_p = BIGENDIAN_P();
	    goto unpack_integer;

	  case 'l':
	    signed_p = 1;
	    integer_size = NATINT_LEN(long, 4);
	    bigendian_p = BIGENDIAN_P();
	    goto unpack_integer;

	  case 'L':
	    signed_p = 0;
	    integer_size = NATINT_LEN(long, 4);
	    bigendian_p = BIGENDIAN_P();
	    goto unpack_integer;

	  case 'q':
	    signed_p = 1;
	    integer_size = NATINT_LEN_Q;
	    bigendian_p = BIGENDIAN_P();
	    goto unpack_integer;

	  case 'Q':
	    signed_p = 0;
	    integer_size = NATINT_LEN_Q;
	    bigendian_p = BIGENDIAN_P();
	    goto unpack_integer;

	  case 'j':
	    signed_p = 1;
	    integer_size = sizeof(intptr_t);
	    bigendian_p = BIGENDIAN_P();
	    goto unpack_integer;

	  case 'J':
	    signed_p = 0;
	    integer_size = sizeof(uintptr_t);
	    bigendian_p = BIGENDIAN_P();
	    goto unpack_integer;

	  case 'n':
	    signed_p = 0;
	    integer_size = 2;
	    bigendian_p = 1;
	    goto unpack_integer;

	  case 'N':
	    signed_p = 0;
	    integer_size = 4;
	    bigendian_p = 1;
	    goto unpack_integer;

	  case 'v':
	    signed_p = 0;
	    integer_size = 2;
	    bigendian_p = 0;
	    goto unpack_integer;

	  case 'V':
	    signed_p = 0;
	    integer_size = 4;
	    bigendian_p = 0;
	    goto unpack_integer;

	  unpack_integer:
	    if (explicit_endian) {
		bigendian_p = explicit_endian == '>';
	    }
            PACK_LENGTH_ADJUST_SIZE(integer_size);
            while (len-- > 0) {
                int flags = bigendian_p ? INTEGER_PACK_BIG_ENDIAN : INTEGER_PACK_LITTLE_ENDIAN;
                VALUE val;
                if (signed_p)
                    flags |= INTEGER_PACK_2COMP;
                val = rb_integer_unpack(s, integer_size, 1, 0, flags);
                UNPACK_PUSH(val);
                s += integer_size;
            }
            PACK_ITEM_ADJUST();
            break;

	  case 'f':
	  case 'F':
	    PACK_LENGTH_ADJUST_SIZE(sizeof(float));
	    while (len-- > 0) {
		float tmp;
		memcpy(&tmp, s, sizeof(float));
		s += sizeof(float);
		UNPACK_PUSH(DBL2NUM((double)tmp));
	    }
	    PACK_ITEM_ADJUST();
	    break;

	  case 'e':
	    PACK_LENGTH_ADJUST_SIZE(sizeof(float));
	    while (len-- > 0) {
		FLOAT_CONVWITH(tmp);
		memcpy(tmp.buf, s, sizeof(float));
		s += sizeof(float);
		VTOHF(tmp);
		UNPACK_PUSH(DBL2NUM(tmp.f));
	    }
	    PACK_ITEM_ADJUST();
	    break;

	  case 'E':
	    PACK_LENGTH_ADJUST_SIZE(sizeof(double));
	    while (len-- > 0) {
		DOUBLE_CONVWITH(tmp);
		memcpy(tmp.buf, s, sizeof(double));
		s += sizeof(double);
		VTOHD(tmp);
		UNPACK_PUSH(DBL2NUM(tmp.d));
	    }
	    PACK_ITEM_ADJUST();
	    break;

	  case 'D':
	  case 'd':
	    PACK_LENGTH_ADJUST_SIZE(sizeof(double));
	    while (len-- > 0) {
		double tmp;
		memcpy(&tmp, s, sizeof(double));
		s += sizeof(double);
		UNPACK_PUSH(DBL2NUM(tmp));
	    }
	    PACK_ITEM_ADJUST();
	    break;

	  case 'g':
	    PACK_LENGTH_ADJUST_SIZE(sizeof(float));
	    while (len-- > 0) {
		FLOAT_CONVWITH(tmp);
		memcpy(tmp.buf, s, sizeof(float));
		s += sizeof(float);
		NTOHF(tmp);
		UNPACK_PUSH(DBL2NUM(tmp.f));
	    }
	    PACK_ITEM_ADJUST();
	    break;

	  case 'G':
	    PACK_LENGTH_ADJUST_SIZE(sizeof(double));
	    while (len-- > 0) {
		DOUBLE_CONVWITH(tmp);
		memcpy(tmp.buf, s, sizeof(double));
		s += sizeof(double);
		NTOHD(tmp);
		UNPACK_PUSH(DBL2NUM(tmp.d));
	    }
	    PACK_ITEM_ADJUST();
	    break;

	  case 'U':
	    if (len > send - s) len = send - s;
	    while (len > 0 && s < send) {
		long alen = send - s;
		unsigned long l;

		l = utf8_to_uv(s, &alen);
		s += alen; len--;
		UNPACK_PUSH(ULONG2NUM(l));
	    }
	    break;

	  case 'u':
	    {
                VALUE buf = rb_str_new(0, (send - s)*3/4);
		char *ptr = RSTRING_PTR(buf);
		long total = 0;

		while (s < send && (unsigned char)*s > ' ' && (unsigned char)*s < 'a') {
		    long a,b,c,d;
		    char hunk[3];

		    len = ((unsigned char)*s++ - ' ') & 077;

		    total += len;
		    if (total > RSTRING_LEN(buf)) {
			len -= total - RSTRING_LEN(buf);
			total = RSTRING_LEN(buf);
		    }

		    while (len > 0) {
			long mlen = len > 3 ? 3 : len;

			if (s < send && (unsigned char)*s >= ' ' && (unsigned char)*s < 'a')
			    a = ((unsigned char)*s++ - ' ') & 077;
			else
			    a = 0;
			if (s < send && (unsigned char)*s >= ' ' && (unsigned char)*s < 'a')
			    b = ((unsigned char)*s++ - ' ') & 077;
			else
			    b = 0;
			if (s < send && (unsigned char)*s >= ' ' && (unsigned char)*s < 'a')
			    c = ((unsigned char)*s++ - ' ') & 077;
			else
			    c = 0;
			if (s < send && (unsigned char)*s >= ' ' && (unsigned char)*s < 'a')
			    d = ((unsigned char)*s++ - ' ') & 077;
			else
			    d = 0;
			hunk[0] = (char)(a << 2 | b >> 4);
			hunk[1] = (char)(b << 4 | c >> 2);
			hunk[2] = (char)(c << 6 | d);
			memcpy(ptr, hunk, mlen);
			ptr += mlen;
			len -= mlen;
		    }
		    if (s < send && (unsigned char)*s != '\r' && *s != '\n')
			s++;	/* possible checksum byte */
		    if (s < send && *s == '\r') s++;
		    if (s < send && *s == '\n') s++;
		}

		rb_str_set_len(buf, total);
		UNPACK_PUSH(buf);
	    }
	    break;

	  case 'm':
	    {
                VALUE buf = rb_str_new(0, (send - s + 3)*3/4); /* +3 is for skipping paddings */
		char *ptr = RSTRING_PTR(buf);
		int a = -1,b = -1,c = 0,d = 0;
		static signed char b64_xtable[256];

		if (b64_xtable['/'] <= 0) {
		    int i;

		    for (i = 0; i < 256; i++) {
			b64_xtable[i] = -1;
		    }
		    for (i = 0; i < 64; i++) {
			b64_xtable[(unsigned char)b64_table[i]] = (char)i;
		    }
		}
		if (len == 0) {
		    while (s < send) {
			a = b = c = d = -1;
			a = b64_xtable[(unsigned char)*s++];
			if (s >= send || a == -1) rb_raise(rb_eArgError, "invalid base64");
			b = b64_xtable[(unsigned char)*s++];
			if (s >= send || b == -1) rb_raise(rb_eArgError, "invalid base64");
			if (*s == '=') {
			    if (s + 2 == send && *(s + 1) == '=') break;
			    rb_raise(rb_eArgError, "invalid base64");
			}
			c = b64_xtable[(unsigned char)*s++];
			if (s >= send || c == -1) rb_raise(rb_eArgError, "invalid base64");
			if (s + 1 == send && *s == '=') break;
			d = b64_xtable[(unsigned char)*s++];
			if (d == -1) rb_raise(rb_eArgError, "invalid base64");
			*ptr++ = castchar(a << 2 | b >> 4);
			*ptr++ = castchar(b << 4 | c >> 2);
			*ptr++ = castchar(c << 6 | d);
		    }
		    if (c == -1) {
			*ptr++ = castchar(a << 2 | b >> 4);
			if (b & 0xf) rb_raise(rb_eArgError, "invalid base64");
		    }
		    else if (d == -1) {
			*ptr++ = castchar(a << 2 | b >> 4);
			*ptr++ = castchar(b << 4 | c >> 2);
			if (c & 0x3) rb_raise(rb_eArgError, "invalid base64");
		    }
		}
		else {
		    while (s < send) {
			a = b = c = d = -1;
			while ((a = b64_xtable[(unsigned char)*s]) == -1 && s < send) {s++;}
			if (s >= send) break;
			s++;
			while ((b = b64_xtable[(unsigned char)*s]) == -1 && s < send) {s++;}
			if (s >= send) break;
			s++;
			while ((c = b64_xtable[(unsigned char)*s]) == -1 && s < send) {if (*s == '=') break; s++;}
			if (*s == '=' || s >= send) break;
			s++;
			while ((d = b64_xtable[(unsigned char)*s]) == -1 && s < send) {if (*s == '=') break; s++;}
			if (*s == '=' || s >= send) break;
			s++;
			*ptr++ = castchar(a << 2 | b >> 4);
			*ptr++ = castchar(b << 4 | c >> 2);
			*ptr++ = castchar(c << 6 | d);
			a = -1;
		    }
		    if (a != -1 && b != -1) {
			if (c == -1)
			    *ptr++ = castchar(a << 2 | b >> 4);
			else {
			    *ptr++ = castchar(a << 2 | b >> 4);
			    *ptr++ = castchar(b << 4 | c >> 2);
			}
		    }
		}
		rb_str_set_len(buf, ptr - RSTRING_PTR(buf));
		UNPACK_PUSH(buf);
	    }
	    break;

	  case 'M':
	    {
                VALUE buf = rb_str_new(0, send - s);
		char *ptr = RSTRING_PTR(buf), *ss = s;
		int csum = 0;
		int c1, c2;

		while (s < send) {
		    if (*s == '=') {
			if (++s == send) break;
			if (s+1 < send && *s == '\r' && *(s+1) == '\n')
			    s++;
			if (*s != '\n') {
			    if ((c1 = hex2num(*s)) == -1) break;
			    if (++s == send) break;
			    if ((c2 = hex2num(*s)) == -1) break;
			    csum |= *ptr++ = castchar(c1 << 4 | c2);
			}
		    }
		    else {
			csum |= *ptr++ = *s;
		    }
		    s++;
		    ss = s;
		}
		rb_str_set_len(buf, ptr - RSTRING_PTR(buf));
		rb_str_buf_cat(buf, ss, send-ss);
		csum = ISASCII(csum) ? ENC_CODERANGE_7BIT : ENC_CODERANGE_VALID;
		ENCODING_CODERANGE_SET(buf, rb_ascii8bit_encindex(), csum);
		UNPACK_PUSH(buf);
	    }
	    break;

	  case '@':
	    if (len > RSTRING_LEN(str))
		rb_raise(rb_eArgError, "@ outside of string");
	    s = RSTRING_PTR(str) + len;
	    break;

	  case 'X':
	    if (len > s - RSTRING_PTR(str))
		rb_raise(rb_eArgError, "X outside of string");
	    s -= len;
	    break;

	  case 'x':
	    if (len > send - s)
		rb_raise(rb_eArgError, "x outside of string");
	    s += len;
	    break;

	  case 'P':
	    if (sizeof(char *) <= (size_t)(send - s)) {
		VALUE tmp = Qnil;
		char *t;

		memcpy(&t, s, sizeof(char *));
		s += sizeof(char *);

		if (t) {
		    VALUE a;
		    const VALUE *p, *pend;

		    if (!(a = str_associated(str))) {
			rb_raise(rb_eArgError, "no associated pointer");
		    }
		    p = RARRAY_CONST_PTR(a);
		    pend = p + RARRAY_LEN(a);
		    while (p < pend) {
			if (RB_TYPE_P(*p, T_STRING) && RSTRING_PTR(*p) == t) {
			    if (len < RSTRING_LEN(*p)) {
                                tmp = rb_str_new(t, len);
				str_associate(tmp, a);
			    }
			    else {
				tmp = *p;
			    }
			    break;
			}
			p++;
		    }
		    if (p == pend) {
			rb_raise(rb_eArgError, "non associated pointer");
		    }
		}
		UNPACK_PUSH(tmp);
	    }
	    break;

	  case 'p':
	    if (len > (long)((send - s) / sizeof(char *)))
		len = (send - s) / sizeof(char *);
	    while (len-- > 0) {
		if ((size_t)(send - s) < sizeof(char *))
		    break;
		else {
		    VALUE tmp = Qnil;
		    char *t;

		    memcpy(&t, s, sizeof(char *));
		    s += sizeof(char *);

		    if (t) {
			VALUE a;
			const VALUE *p, *pend;

			if (!(a = str_associated(str))) {
			    rb_raise(rb_eArgError, "no associated pointer");
			}
			p = RARRAY_CONST_PTR(a);
			pend = p + RARRAY_LEN(a);
			while (p < pend) {
			    if (RB_TYPE_P(*p, T_STRING) && RSTRING_PTR(*p) == t) {
				tmp = *p;
				break;
			    }
			    p++;
			}
			if (p == pend) {
			    rb_raise(rb_eArgError, "non associated pointer");
			}
		    }
		    UNPACK_PUSH(tmp);
		}
	    }
	    break;

	  case 'w':
	    {
                char *s0 = s;
                while (len > 0 && s < send) {
                    if (*s & 0x80) {
                        s++;
                    }
                    else {
                        s++;
                        UNPACK_PUSH(rb_integer_unpack(s0, s-s0, 1, 1, INTEGER_PACK_BIG_ENDIAN));
                        len--;
                        s0 = s;
                    }
                }
	    }
	    break;

	  default:
            unknown_directive("unpack", type, fmt);
	    break;
	}
    }

    *parsed_len = s - init_s;
    return ary;
}

int
srb_uv_to_utf8(char buf[6], unsigned long uv)
{
    if (uv <= 0x7f) {
	buf[0] = (char)uv;
	return 1;
    }
    if (uv <= 0x7ff) {
	buf[0] = castchar(((uv>>6)&0xff)|0xc0);
	buf[1] = castchar((uv&0x3f)|0x80);
	return 2;
    }
    if (uv <= 0xffff) {
	buf[0] = castchar(((uv>>12)&0xff)|0xe0);
	buf[1] = castchar(((uv>>6)&0x3f)|0x80);
	buf[2] = castchar((uv&0x3f)|0x80);
	return 3;
    }
    if (uv <= 0x1fffff) {
	buf[0] = castchar(((uv>>18)&0xff)|0xf0);
	buf[1] = castchar(((uv>>12)&0x3f)|0x80);
	buf[2] = castchar(((uv>>6)&0x3f)|0x80);
	buf[3] = castchar((uv&0x3f)|0x80);
	return 4;
    }
    if (uv <= 0x3ffffff) {
	buf[0] = castchar(((uv>>24)&0xff)|0xf8);
	buf[1] = castchar(((uv>>18)&0x3f)|0x80);
	buf[2] = castchar(((uv>>12)&0x3f)|0x80);
	buf[3] = castchar(((uv>>6)&0x3f)|0x80);
	buf[4] = castchar((uv&0x3f)|0x80);
	return 5;
    }
    if (uv <= 0x7fffffff) {
	buf[0] = castchar(((uv>>30)&0xff)|0xfc);
	buf[1] = castchar(((uv>>24)&0x3f)|0x80);
	buf[2] = castchar(((uv>>18)&0x3f)|0x80);
	buf[3] = castchar(((uv>>12)&0x3f)|0x80);
	buf[4] = castchar(((uv>>6)&0x3f)|0x80);
	buf[5] = castchar((uv&0x3f)|0x80);
	return 6;
    }
    rb_raise(rb_eRangeError, "pack(U): value out of range");

    UNREACHABLE_RETURN(Qnil);
}

static const unsigned long utf8_limits[] = {
    0x0,			/* 1 */
    0x80,			/* 2 */
    0x800,			/* 3 */
    0x10000,			/* 4 */
    0x200000,			/* 5 */
    0x4000000,			/* 6 */
    0x80000000,			/* 7 */
};

static unsigned long
utf8_to_uv(const char *p, long *lenp)
{
    int c = *p++ & 0xff;
    unsigned long uv = c;
    long n;

    if (!(uv & 0x80)) {
	*lenp = 1;
        return uv;
    }
    if (!(uv & 0x40)) {
	*lenp = 1;
	rb_raise(rb_eArgError, "malformed UTF-8 character");
    }

    if      (!(uv & 0x20)) { n = 2; uv &= 0x1f; }
    else if (!(uv & 0x10)) { n = 3; uv &= 0x0f; }
    else if (!(uv & 0x08)) { n = 4; uv &= 0x07; }
    else if (!(uv & 0x04)) { n = 5; uv &= 0x03; }
    else if (!(uv & 0x02)) { n = 6; uv &= 0x01; }
    else {
	*lenp = 1;
	rb_raise(rb_eArgError, "malformed UTF-8 character");
    }
    if (n > *lenp) {
	rb_raise(rb_eArgError, "malformed UTF-8 character (expected %ld bytes, given %ld bytes)",
		 n, *lenp);
    }
    *lenp = n--;
    if (n != 0) {
	while (n--) {
	    c = *p++ & 0xff;
	    if ((c & 0xc0) != 0x80) {
		*lenp -= n + 1;
		rb_raise(rb_eArgError, "malformed UTF-8 character");
	    }
	    else {
		c &= 0x3f;
		uv = uv << 6 | c;
	    }
	}
    }
    n = *lenp - 1;
    if (uv < utf8_limits[n]) {
	rb_raise(rb_eArgError, "redundant UTF-8 sequence");
    }
    return uv;
}
