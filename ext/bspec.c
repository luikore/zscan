#include "zscan.h"

static const rb_data_type_t* zscan_type;
static void** bspec_opcodes;
# include "bspec_init.inc"

typedef struct {
  long s_size;
  long a_size;
  long a_cap;
  void** code;
} BSpec;

static void bspec_free(void* pp) {
  BSpec* p = pp;
  free(p->code);
  free(p);
}

static size_t bspec_memsize(const void* pp) {
  const BSpec* p = pp;
  return p ? sizeof(*p) : 0;
}

static const rb_data_type_t bspec_type = {
  "ZScan::BinarySpec",
  {NULL, bspec_free, bspec_memsize}
};

static VALUE bspec_alloc(VALUE klass) {
  BSpec* bs = (BSpec*)malloc(sizeof(BSpec));
  bs->s_size = 0;
  bs->a_cap = 4;
  bs->a_size = 0;
  bs->code = (void**)malloc(bs->a_cap * sizeof(void*));
  for (long i = 0; i < bs->a_cap; i++) {
    bs->code[i] = bspec_opcodes[0];
  }
  return TypedData_Wrap_Struct(klass, &bspec_type, bs);
}

static VALUE bspec_append(VALUE self, VALUE v_i) {
  BSpec* bs = rb_check_typeddata(self, &bspec_type);
  long i = NUM2LONG(v_i);
  if (i < 0 || i >= bspec_opcodes_size) {
    rb_raise(rb_eArgError, "bad opcode index");
  }

  // ensure size
  if (bs->a_size == bs->a_cap - 1) { // end with 0:RET, always terminate
    bs->code = (void**)realloc(bs->code, bs->a_cap * 2 * sizeof(void*));
    long j = bs->a_cap;
    bs->a_cap *= 2;
    for (; j < bs->a_cap; j++) {
      bs->code[j] = bspec_opcodes[0];
    }
  }

  bs->code[bs->a_size++] = bspec_opcodes[i];
  bs->s_size += bspec_s_sizes[i];
  return self;
}

static VALUE bspec_big_endian_p(VALUE self) {
# ifdef DYNAMIC_ENDIAN
  /* for universal binary of NEXTSTEP and MacOS X */
  int init = 1;
  char* p = (char*)&init;
  return p[0] ? Qfalse : Qtrue;
# elif defined(WORDS_BIGENDIAN)
  return Qtrue;
#else
  return Qfalse;
#endif
}

#define GCC_VERSION_SINCE(major, minor, patchlevel) \
  (defined(__GNUC__) && !defined(__INTEL_COMPILER) && \
   ((__GNUC__ > (major)) ||  \
    (__GNUC__ == (major) && __GNUC_MINOR__ > (minor)) || \
    (__GNUC__ == (major) && __GNUC_MINOR__ == (minor) && __GNUC_PATCHLEVEL__ >= (patchlevel))))

#if GCC_VERSION_SINCE(4,3,0) || defined(__clang__)
# define swap32(x) __builtin_bswap32(x)
# define swap64(x) __builtin_bswap64(x)
#endif

#ifndef swap16
# define swap16(x)  ((uint16_t)((((x)&0xFF)<<8) | (((x)>>8)&0xFF)))
#endif

#ifndef swap32
# define swap32(x)  ((uint32_t)((((x)&0xFF)<<24) \
    |(((x)>>24)&0xFF)      \
    |(((x)&0x0000FF00)<<8) \
    |(((x)&0x00FF0000)>>8)))
#endif

#ifndef swap64
# define byte_in_64bit(n) ((uint64_t)0xff << (n))
# define swap64(x)  ((uint64_t)((((x)&byte_in_64bit(0))<<56) \
    |(((x)>>56)&0xFF)              \
    |(((x)&byte_in_64bit(8))<<40)  \
    |(((x)&byte_in_64bit(48))>>40) \
    |(((x)&byte_in_64bit(16))<<24) \
    |(((x)&byte_in_64bit(40))>>24) \
    |(((x)&byte_in_64bit(24))<<8)  \
    |(((x)&byte_in_64bit(32))>>8)))
#endif

// NOTE can not use sizeof in preprocessor
#define INT64toNUM(x) (sizeof(long) == 8 ? LONG2NUM(x) : LL2NUM(x))
#define UINT64toNUM(x) (sizeof(long) == 8 ? ULONG2NUM(x) : ULL2NUM(x))

#define CAST(var, ty) *((ty*)(&(var)))

#include "bspec_exec.inc"

static VALUE bspec_opcodes_to_a(VALUE bspec) {
  volatile VALUE a = rb_ary_new();
  for (long i = 0; i < bspec_opcodes_size; i++) {
    rb_ary_push(a, UINT64toNUM((uint64_t)(bspec_opcodes[i])));
  }
  return a;
}

static VALUE bspec_inspect_opcodes(VALUE bspec, VALUE self) {
  BSpec* bs = rb_check_typeddata(self, &bspec_type);
  volatile VALUE a = rb_ary_new();
  for (long i = 0; i <= bs->a_size; i++) {
    rb_ary_push(a, UINT64toNUM((uint64_t)(bs->code[i])));
  }
  return a;
}

static VALUE zscan_scan_binary(VALUE self, VALUE spec) {
  ZScan* p = rb_check_typeddata(self, zscan_type);
  BSpec* bs = rb_check_typeddata(spec, &bspec_type);
  if (bs->a_size == 0) {
    return rb_ary_new();
  }
  long s_size = bs->s_size;
  if (p->bytepos + s_size > RSTRING_LEN(p->s)) {
    return Qnil;
  }
  volatile VALUE a = rb_ary_new2(bs->a_size - 1);
  bspec_exec(bs->code, RSTRING_PTR(p->s) + p->bytepos, a);
  zscan_bytepos_eq(self, LONG2NUM(p->bytepos + s_size));
  return a;
}

void Init_zscan_bspec(VALUE zscan, const rb_data_type_t* _zscan_type) {
  zscan_type = _zscan_type;
  rb_define_method(zscan, "scan_binary", zscan_scan_binary, 1);

  bspec_opcodes = (void**)bspec_exec(NULL, NULL, Qnil);
  VALUE bs = rb_define_class_under(zscan, "BinarySpec", rb_cObject);
  rb_define_singleton_method(bs, "big_endian?", bspec_big_endian_p, 0);
  rb_define_singleton_method(bs, "_opcodes_to_a", bspec_opcodes_to_a, 0);
  rb_define_singleton_method(bs, "_inspect_opcodes", bspec_inspect_opcodes, 1);
  rb_define_alloc_func(bs, bspec_alloc);
  rb_define_method(bs, "append", bspec_append, 1);
}
