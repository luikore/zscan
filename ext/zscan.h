#pragma once
#include <ruby/ruby.h>
#include <ruby/re.h>
#include <ruby/encoding.h>
#include <ctype.h>

typedef struct {
  long pos;
  long bytepos;
} Pos;

typedef struct {
  long pos;
  long bytepos;
  VALUE s;
  long stack_i;
  long stack_cap;
  Pos* stack;
} ZScan;

VALUE zscan_bytepos_eq(VALUE self, VALUE v_bytepos);
VALUE zscan_internal_unpack(VALUE str, VALUE fmt, long* parsed_len);
void Init_zscan_bspec(VALUE, const rb_data_type_t*);
