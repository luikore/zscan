#include "zscan.h"
// todo infect check

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

#define P ZScan* p = rb_check_typeddata(self, &zscan_type)

static VALUE zscan_alloc(VALUE klass) {
  ZScan* p = ALLOC(ZScan);
  MEMZERO(p, ZScan, 1);
  p->s = Qnil;
  p->stack_cap = 8;
  p->stack = (Pos*)malloc(sizeof(Pos) * p->stack_cap);
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
  long n = p->pos + NUM2LONG(v_diff);
  if (n < 0) {
    p->pos = 0;
    p->bytepos = 0;
    return self;
  }

  // because there's no "reverse scan" API, we have a O(n) routine :(
  if (n < p->pos) {
    p->pos = 0;
    p->bytepos = 0;
  }

  if (n > p->pos) {
    rb_encoding* enc = rb_enc_get(p->s);
    long byteend = RSTRING_LEN(p->s);
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
  long from, to, bytepos;

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
  long diff = 0;
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
  return (p->bytepos == RSTRING_LEN(p->s) ? Qtrue : Qfalse);
}

regex_t *rb_reg_prepare_re(VALUE re, VALUE str);
static VALUE zscan_match_bytesize(VALUE self, VALUE pattern) {
  P;
  if (TYPE(pattern) == T_STRING) {
    volatile VALUE ss = rb_funcall(p->s, rb_intern("byteslice"), 2, ULONG2NUM(p->bytepos), ULONG2NUM(RSTRING_LEN(p->s)));
    if (RTEST(rb_funcall(ss, rb_intern("start_with?"), 1, pattern))) {
      return ULONG2NUM(RSTRING_LEN(pattern));
    }
  } else if (TYPE(pattern) == T_REGEXP) {
    regex_t *re = rb_reg_prepare_re(pattern, p->s); // prepare with compatible encoding
    int tmpreg = re != RREGEXP(pattern)->ptr;
    if (!tmpreg) {
      RREGEXP(pattern)->usecnt++;
    }

    char* ptr = RSTRING_PTR(p->s);
    UChar* ptr_end = (UChar*)(ptr + RSTRING_LEN(p->s));
    UChar* ptr_match_from = (UChar*)(ptr + p->bytepos);
    long ret = onig_match(re, (UChar*)ptr, ptr_end, ptr_match_from, NULL, ONIG_OPTION_NONE);

    if (tmpreg) {
      if (RREGEXP(pattern)->usecnt) {
        onig_free(re);
      } else {
        onig_free(RREGEXP(pattern)->ptr);
        RREGEXP(pattern)->ptr = re;
      }
    } else {
      RREGEXP(pattern)->usecnt--;
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

static VALUE zscan_scan(VALUE self, VALUE pattern) {
  VALUE v_bytelen = zscan_match_bytesize(self, pattern);
  if (v_bytelen == Qnil) {
    return Qnil;
  } else {
    P;
    long bytelen = NUM2LONG(v_bytelen);
    volatile VALUE ret = rb_funcall(p->s, rb_intern("byteslice"), 2, LONG2NUM(p->bytepos), v_bytelen);
    VALUE v_len = rb_str_length(ret);
    p->bytepos += bytelen;
    p->pos += NUM2LONG(v_len);
    return ret;
  }
}

static VALUE zscan_push(VALUE self) {
  P;
  if (p->stack_i + 1 == p->stack_cap) {
    p->stack_cap = p->stack_cap * 1.4 + 3;
    p->stack = (Pos*)realloc(p->stack, sizeof(Pos) * p->stack_cap);
  }
  Pos e = {p->pos, p->bytepos};
  p->stack[++p->stack_i] = e;
  return self;
}

static VALUE zscan_pop(VALUE self) {
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

static VALUE zscan_drop(VALUE self) {
  P;
  if (p->stack_i) {
    p->stack_i--;
  }
  return self;
}

static VALUE zscan_restore(VALUE self) {
  P;
  if (p->stack_i) {
    p->pos = p->stack[p->stack_i].pos;
    p->bytepos = p->stack[p->stack_i].bytepos;
  }
  return self;
}

static VALUE zscan_clear_pos_stack(VALUE self) {
  P;
  p->stack_i = 0;
  return self;
}

#define REQUIRE_BLOCK \
  if (!rb_block_given_p()) {\
    rb_raise(rb_eRuntimeError, "need a block");\
  }

static VALUE zscan_try(VALUE self) {
  REQUIRE_BLOCK;
  VALUE r;
  zscan_push(self);
  r = rb_yield(Qnil);
  if (RTEST(r)) {
    zscan_drop(self);
  } else {
    zscan_pop(self);
  }
  return r;
}

// optimized version without pushing and block
static VALUE zscan__try(VALUE self, VALUE r) {
  if (RTEST(r)) {
    zscan_drop(self);
  } else {
    zscan_pop(self);
  }
  return r;
}

static VALUE zscan_zero_or_one(int argc, VALUE* argv, VALUE self) {
  REQUIRE_BLOCK;
  volatile VALUE a = Qnil;
  volatile VALUE r;
  rb_scan_args(argc, argv, "01", &a);
  if (a == Qnil) {
    a = rb_ary_new();
  }
  zscan_push(self);
  r = rb_yield(Qnil);
  if (RTEST(r)) {
    rb_funcall(a, rb_intern("<<"), 1, r);
    zscan_drop(self);
  } else {
    zscan_pop(self);
  }
  return a;
}

static VALUE zscan_zero_or_more(int argc, VALUE* argv, VALUE self) {
  REQUIRE_BLOCK;
  volatile VALUE a = Qnil;
  volatile VALUE r;
  long backpos;
  P;
  rb_scan_args(argc, argv, "01", &a);
  if (a == Qnil) {
    a = rb_ary_new();
  }
  for (;;) {
    zscan_push(self);
    backpos = p->bytepos;
    r = rb_yield(Qnil);
    if (RTEST(r) && backpos != p->bytepos) {
      rb_funcall(a, rb_intern("<<"), 1, r);
      zscan_drop(self);
    } else {
      zscan_pop(self);
      break;
    }
  }
  return a;
}

static VALUE zscan_one_or_more(int argc, VALUE* argv, VALUE self) {
  REQUIRE_BLOCK;
  volatile VALUE a = Qnil;
  volatile VALUE r;

  r = rb_yield(Qnil);
  if (RTEST(r)) {
    long backpos;
    P;
    rb_scan_args(argc, argv, "01", &a);
    if (a == Qnil) {
      a = rb_ary_new();
    }

    rb_funcall(a, rb_intern("<<"), 1, r);
    for (;;) {
      zscan_push(self);
      backpos = p->bytepos;
      r = rb_yield(Qnil);
      if (RTEST(r) && backpos != p->bytepos) {
        rb_funcall(a, rb_intern("<<"), 1, r);
        zscan_drop(self);
      } else {
        zscan_pop(self);
        break;
      }
    }
    return a;
  } else {
    return Qnil;
  }
}

VALUE zscan_scan_float(VALUE self) {
  P;
  if (RSTRING_LEN(p->s) == p->bytepos) {
    return Qnil;
  }

  char* s = RSTRING_PTR(p->s) + p->bytepos;
  if (isspace(s[0])) {
    return Qnil;
  }
  char* e;
  double d = strtod(s, &e);
  if (e == s || e - s > RSTRING_LEN(p->s) - p->bytepos) {
    return Qnil;
  } else {
    // it ok to use advance because the source is ascii compatible
    zscan_advance(self, LONG2NUM(e - s));
    return DBL2NUM(d);
  }
}

extern void Init_zscan_bspec(VALUE, const rb_data_type_t*);

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
  rb_define_method(zscan, "match_bytesize", zscan_match_bytesize, 1);
  rb_define_method(zscan, "scan", zscan_scan, 1);
  rb_define_method(zscan, "push", zscan_push, 0);
  rb_define_method(zscan, "pop", zscan_pop, 0);
  rb_define_method(zscan, "drop", zscan_drop, 0);
  rb_define_method(zscan, "restore", zscan_restore, 0);
  rb_define_method(zscan, "clear_pos_stack", zscan_clear_pos_stack, 0);

  rb_define_method(zscan, "try", zscan_try, 0);
  rb_define_method(zscan, "_try", zscan__try, 1);
  rb_define_method(zscan, "zero_or_one", zscan_zero_or_one, -1);
  rb_define_method(zscan, "zero_or_more", zscan_zero_or_more, -1);
  rb_define_method(zscan, "one_or_more", zscan_one_or_more, -1);

  rb_define_method(zscan, "scan_float", zscan_scan_float, 0);
  Init_zscan_bspec(zscan, &zscan_type);
}
