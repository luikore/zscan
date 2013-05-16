#include "zscan.h"

static const rb_data_type_t* zscan_type;

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
  return TypedData_Wrap_Struct(klass, &bspec_type, bs);
}

static VALUE bspec_append(VALUE self, VALUE v_code, VALUE v_s_size) {
  BSpec* bs = rb_check_typeddata(self, &bspec_type);
  if (bs->a_size == bs->a_cap) {
    bs->a_cap *= 2;
    bs->code = (void**)realloc(bs->code, bs->a_cap * sizeof(void*));
  }
  long s_size = NUM2LONG(v_s_size);
  bs->code[bs->a_size++] = ((void**)RSTRING_PTR(v_code))[0];
  bs->s_size += s_size;
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

static VALUE zscan_scan_binary(VALUE self, VALUE spec) {
  ZScan* p = rb_check_typeddata(self, zscan_type);
  if (!rb_enc_str_asciicompat_p(p->s)) {
    rb_raise(rb_eRuntimeError, "encoding of source string should be ascii-compatible");
    return Qnil;
  }
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
  p->bytepos += s_size;
  p->pos += s_size;
  return a;
}

void Init_zscan_bspec(VALUE zscan, const rb_data_type_t* _zscan_type) {
  zscan_type = _zscan_type;
  rb_define_method(zscan, "scan_binary", zscan_scan_binary, 1);

  VALUE bs = rb_define_class_under(zscan, "BinarySpec", rb_cObject);
  rb_define_singleton_method(bs, "big_endian?", bspec_big_endian_p, 0);
  rb_define_alloc_func(bs, bspec_alloc);
  rb_define_method(bs, "append", bspec_append, 2);

# include "bspec_opcode_names.inc"
  void** opcodes = (void**)bspec_exec(NULL, NULL, Qnil);
  for (long i = 0; i < bspec_opcode_size; i++) {
    VALUE bytecode = rb_str_new((char*)&opcodes[i], sizeof(void*));
    OBJ_FREEZE(bytecode);
    rb_define_const(bs, bspec_opcode_names[i], bytecode);
  }
}
