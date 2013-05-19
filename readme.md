## Features

- `ZScan#scan`/`ZScan#skip`/`ZScan#match_bytesize` accept either string or regexp as param.
- `ZScan#pos` is the codepoint position, and `ZScan#bytepos` is byte position.
- Correctly scans anchors and look behind predicates.
- Pos stack manipulation.
- Typed scanning methods: `#scan_float`, `#scan_int radix=nil`, `#scan_date format`, `#scan_binary format`.

## Install

```bash
gem ins zscan
```

## Typical use

``` ruby
require 'zscan'
z = ZScan.new 'hello world'
z.scan 'hello' #=> 'hello'
z.skip ' '
z.scan /\w+/   #=> 'world'
z.eos?         #=> true
```

## Motivation: string scanner

Ruby's stdlib `StringScanner` treats the scanning position as beginning of string:

```ruby
require 'strscan'
s = StringScanner.new 'ab'
s.pos = 1
s.scan /(?<a)/ #=> nil
s.scan /^/     #=> ''
```

But for building parser generators, I need the scanner check the whole string for anchors and lookbehinds:

```ruby
require 'zscan'
z = ZScan.new 'ab'
z.pos = 1
z.scan /(?<a)/ #=> ''
z.scan /^/     #=> nil
```

See also https://bugs.ruby-lang.org/issues/7092

## Other motivations

- For scan and convert, ruby's stdlib `Scanf` is slow (creates regexp array everytime called) and not possible to corporate with scanner.
- For date parsing, `strptime` doesn't tell the parsed length.
- For binary parsing, `unpack` is an slow interpreter, and the instructions are quite irregular.

## Essential methods

- `ZScan.new string, dup=false`
- `#scan regexp_or_string`
- `#skip regexp_or_string`
- `#match_bytesize regexp_or_string` return length of matched bytes or `nil`.
- `#slice n` slice sub string of n chars from current pos, advances the cursor.
- `#byteslice n` slice sub string of n bytes from cursor pos, advances the cursor.
- `#scan_float` scan a float number which is not starting with space. It deals with multibyte encodings for you.
- `#scan_int radix=nil` if radix is nil, decide base by prefix: `0x` is 16, `0` is 8, `0b` is 2, otherwise 10. `radix` should be in range `2..36`.
- `#scan_date format_string, start=Date::ITALY` scan a `DateTime` object, see also [strptime](http://rubydoc.info/stdlib/date/DateTime.strptime).
- `#scan_binary binary_spec` optimized and readable binary scan, see below for how to create a `ZScan::BinarySpec`.
- `#unpack format_string` note that unpack always returns an array no matter matched or not (same behavior as String#unpack).
- `#eos?`
- `#string` note: return a dup. Don't worry the performance because it is a copy-on-write string.
- `#rest` rest unscanned sub string.
- `#rest_size` count characters of unscanned sub string.
- `#rest_bytesize` count bytes of unscanned sub string.

## String delegates

For convienience

- `#<< append_string`
- `#[]= range, replace_string` note: if `range` starts before pos, moves pos left, also clears the stack.
- `#size`
- `#bytesize`

## Pos management

- `#pos`
- `#pos= new_pos` note: complexity ~ `new_pos > pos ? new_pos - pos : new_pos`.
- `#bytepos`
- `#bytepos= new_bytepos` note: complexity ~ `abs(new_bytepos - bytepos)`.
- `#line_index` line index of current position, start from `0`.
- `#advance n` move forward `n` codepoints, if `n < 0`, move backward. Stops at beginning or end.
- `#reset` go to beginning.
- `#terminate` go to end of string.

## Binary parsing

Designed for binary protocol parsing. You can specify a sequence of binary data and how to expect the matching. Example:

```ruby
s = ZScan.BinarySpec.new do
  int8        # once
  uint32_le 2 # little endian, twice
  double_be 1 # big endian, once
end
z = ZScan.new [-1, 2, 3, 4.0].pack('cI<2G') + "rest"
z.scan_binary s #=> [-1, 2, 3, 4.0]
z.rest #=> 'rest
```

Integer instructions:

```ruby
int8  uint8
int16 uint16 int16_le uint16_le int16_be uint16_be
int32 uint32 int32_le uint32_le int32_be uint32_be
int64 uint64 int64_le uint64_le int64_be uint64_be
```

Single precision float instructions:

```ruby
single single_le single_be
```

Double precision float instructions:

```ruby
double double_le double_be
```

Endians:

- (without endian suffix) native endian
- `*_le` little endian (VAX, x86, Windows string code unit)
- `*_be` big endian, network endian (SPARC, Java string code unit)

Repeat count must be integer `>= 1`, default is `1`.

It is implemented as a direct-threaded bytecode interpreter. A bit faster than `String#unpack`.

## Parsing combinators

Combinators that manage scanner pos and stack state for you. In the combinators, if the returned value of the given block is `nil` or `false`, stops iteration and restores scanner location. Can be nested, useful for building parsers.

- `#try &block` returns `block`'s return.
- `#zero_or_one acc=[], &block` try to execute 0 or 1 time, returns `acc`.
- `#zero_or_more acc=[], &block` try to execute 0 or more times, also stops iteration if scanner no advance, returns `acc`.
- `#one_or_more acc=[], &block` try to execute 1 or more times, also stops iteration if scanner no advance, returns `nil` or `acc`.

## (Low level) Efficient pos stack manipulation

- `#push` push current pos into the stack.
- `#pop` set current pos to top of the stack, and pop it.
- `#drop` drop top of pos stack without changing current pos.
- `#restore` set current pos to top of the stack.
- `#clear_pos_stack` clear pos stack.
- `z.push._try expr` equivalent to `z.try{ expr }`, but faster because no block is required

## License

```
Copyright (C) 2013 by Zete Lui (BSD)

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
```
