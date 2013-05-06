#include <ruby/ruby.h>
#include <ruby/re.h>
#include <ruby/encoding.h>

typedef struct {
  size_t pos;
  size_t bytepos;
} Pos;

typedef struct {
  size_t pos;
  size_t bytepos;
  VALUE s;
  size_t stack_i;
  size_t stack_cap;
  Pos* stack;
} ZScan;

#define P ZScan* p = rb_check_typeddata(self, &zscan_type)

static void zscan_mark(void* pp) {
  ZScan* p = pp;
  rb_gc_mark(p->s);
}

static void zscan_free(void* pp) {
  ZScan* p = pp;
  free(p->stack);
  ruby_xfree(p);
}

static size_t zscan_memsize(const void* pp) {
  const ZScan* p = pp;
  return p ? sizeof(*p) : 0;
}

static const rb_data_type_t zscan_type = {
  "ZScan",
  {zscan_mark, zscan_free, zscan_memsize}
};

static VALUE zscan_alloc(VALUE klass) {
  ZScan* p = ALLOC(ZScan);
  MEMZERO(p, ZScan, 1);
  p->s = Qnil;
  p->stack_cap = 5;
  p->stack = (Pos*)malloc(sizeof(Pos) * 5);
  return TypedData_Wrap_Struct(klass, &zscan_type, p);
}

static VALUE zscan_internal_init(VALUE self, VALUE v_s) {
  P;
  p->s = v_s;
  return self;
}

static VALUE zscan_internal_string(VALUE self) {
  P;
  return p->s;
}

static VALUE zscan_pos(VALUE self) {
  P;
  return ULONG2NUM(p->pos);
}

static VALUE zscan_advance(VALUE self, VALUE v_diff) {
  P;
  long signed_n = p->pos + NUM2LONG(v_diff);
  if (signed_n < 0) {
    p->pos = 0;
    p->bytepos = 0;
    return self;
  }
  size_t n = signed_n;

  // because there's no "reverse scan" API, we have a O(n) routine :(
  if (n < p->pos) {
    p->pos = 0;
    p->bytepos = 0;
  }

  if (n > p->pos) {
    rb_encoding* enc = rb_enc_get(p->s);
    size_t byteend = RSTRING_LEN(p->s);
    char* ptr = RSTRING_PTR(p->s);
    for (; p->pos < n && p->bytepos < byteend;) {
      int n = rb_enc_mbclen(ptr + p->bytepos, ptr + byteend, enc);
      if (n) {
        p->pos++;
        p->bytepos += n;
      } else {
        break;
      }
    }
  }
  return self;
}

static VALUE zscan_bytepos(VALUE self) {
  P;
  return ULONG2NUM(p->bytepos);
}

static VALUE zscan_bytepos_eq(VALUE self, VALUE v_bytepos) {
  P;
  long signed_bytepos = NUM2LONG(v_bytepos);
  size_t from, to, bytepos;

  if (signed_bytepos > RSTRING_LEN(p->s)) {
    bytepos = RSTRING_LEN(p->s);
  } else if (signed_bytepos < 0) {
    bytepos = 0;
  } else {
    bytepos = signed_bytepos;
  }

  if (bytepos > p->bytepos) {
    from = p->bytepos;
    to = bytepos;
  } else if (bytepos < p->bytepos) {
    from = bytepos;
    to = p->bytepos;
  } else {
    return v_bytepos;
  }

  rb_encoding* enc = rb_enc_get(p->s);
  char* ptr = RSTRING_PTR(p->s);
  size_t diff = 0;
  for (; from < to;) {
    int n = rb_enc_mbclen(ptr + from, ptr + to, enc);
    if (n) {
      diff++;
      from += n;
    } else {
      if (from < to) {
        rb_raise(rb_eRuntimeError, "the given bytepos splits character");
        return v_bytepos;
      }
      break;
    }
  }

  if (bytepos > p->bytepos) {
    p->pos += diff;
  } else if (bytepos < p->bytepos) {
    p->pos -= diff;
  }
  p->bytepos = bytepos;

  return v_bytepos;
}

static VALUE zscan_eos_p(VALUE self) {
  P;
  return (p->bytepos == (size_t)RSTRING_LEN(p->s) ? Qtrue : Qfalse);
}

regex_t *rb_reg_prepare_re(VALUE re, VALUE str);
static VALUE zscan_bmatch_p(VALUE self, VALUE pattern) {
  P;
  if (TYPE(pattern) == T_STRING) {
    volatile VALUE ss = rb_funcall(self, rb_intern("rest"), 0);
    if (RTEST(rb_funcall(ss, rb_intern("start_with?"), 1, pattern))) {
      return ULONG2NUM(RSTRING_LEN(pattern));
    }
  } else if (TYPE(pattern) == T_REGEXP) {
    regex_t *re = rb_reg_prepare_re(pattern, p->s);
    int tmpreg = re != RREGEXP(pattern)->ptr;
    if (!tmpreg) {
      RREGEXP(pattern)->usecnt++;
    }

    char* ptr = RSTRING_PTR(p->s);
    UChar* ptr_end = (UChar*)(ptr + RSTRING_LEN(p->s));
    UChar* ptr_match_from = (UChar*)(ptr + p->bytepos);
    long ret = onig_match(re, (UChar*)ptr, ptr_end, ptr_match_from, NULL, ONIG_OPTION_NONE);

    if (!tmpreg) {
      RREGEXP(pattern)->usecnt--;
    }
    if (tmpreg) {
      if (RREGEXP(pattern)->usecnt) {
        onig_free(re);
      } else {
        onig_free(RREGEXP(pattern)->ptr);
        RREGEXP(pattern)->ptr = re;
      }
    }

    if (ret == -2) {
      rb_raise(rb_eRuntimeError, "regexp buffer overflow");
    } else if (ret >= 0) {
      return LONG2NUM(ret);
    }
  } else {
    rb_raise(rb_eTypeError, "expect String or Regexp");
  }

  return Qnil;
}

static VALUE zscan_push_pos(VALUE self) {
  P;
  if (p->stack_i + 1 == p->stack_cap) {
    p->stack_cap *= 2;
    p->stack = (Pos*)realloc(p->stack, sizeof(Pos) * p->stack_cap);
  }
  Pos e = {p->pos, p->bytepos};
  p->stack[++p->stack_i] = e;
  return self;
}

static VALUE zscan_pop_pos(VALUE self) {
  P;
  if (p->stack_i) {
    p->pos = p->stack[p->stack_i].pos;
    p->bytepos = p->stack[p->stack_i].bytepos;
    p->stack_i--;
  } else {
    p->pos = 0;
    p->bytepos = 0;
  }
  return self;
}

static VALUE zscan_drop_top(VALUE self) {
  P;
  if (p->stack_i) {
    p->stack_i--;
  }
  return self;
}

static VALUE zscan_resume_top(VALUE self) {
  P;
  if (p->stack_i) {
    p->pos = p->stack[p->stack_i].pos;
    p->bytepos = p->stack[p->stack_i].bytepos;
  }
  return self;
}

void Init_zscan() {
  VALUE zscan = rb_define_class("ZScan", rb_cObject);
  rb_define_alloc_func(zscan, zscan_alloc);
  rb_define_method(zscan, "_internal_init", zscan_internal_init, 1);
  rb_define_method(zscan, "_internal_string", zscan_internal_string, 0);

  rb_define_method(zscan, "pos", zscan_pos, 0);
  rb_define_method(zscan, "bytepos", zscan_bytepos, 0);
  rb_define_method(zscan, "bytepos=", zscan_bytepos_eq, 1);
  rb_define_method(zscan, "advance", zscan_advance, 1);
  rb_define_method(zscan, "eos?", zscan_eos_p, 0);
  rb_define_method(zscan, "bmatch?", zscan_bmatch_p, 1);
  rb_define_method(zscan, "push_pos", zscan_push_pos, 0);
  rb_define_method(zscan, "pop_pos", zscan_pop_pos, 0);
  rb_define_method(zscan, "drop_top", zscan_drop_top, 0);
  rb_define_method(zscan, "resume_top", zscan_resume_top, 0);
}
