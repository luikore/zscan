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
