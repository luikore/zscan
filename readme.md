## Motivation

A simple string scanner. Provides... much less methods than `StringScanner`.

It supports either string or regexp as scan param.

`pos` is by codepoints instead of bytes, use `bytepos` to locate byte position.

It provides a position stack for you to efficiently manage scanning locations.

It correctly scans anchors. The following codes demonstrate the behavior:

```ruby
require 'zscan'
z = ZScan.new 'ab'
z.pos = 1
z.scan /(?<a)/ #=> ''
z.scan /^/     #=> nil
```

While with `StringScanner`:

```ruby
require 'strscan'
s = StringScanner.new 'ab'
s.pos = 1
s.scan /(?<a)/ #=> nil
s.scan /^/     #=> ''
```

See also https://bugs.ruby-lang.org/issues/7092

## Methods

- `ZScan.new string, dup=false`
- `scan regexp_or_string`
- `skip regexp_or_string`
- `bmatch? regexp_or_string` returns length of matched bytes or nil
- `eos?`
- `string` note: returns a COW dup
- `rest`

## Position management

- `pos`
- `pos= new_pos` note: complexity ~ `new_pos > pos ? new_pos - pos : new_pos`.
- `bytepos`
- `bytepos= new_bytepos` note: complexity ~ `abs(new_bytepos - bytepos)`.
- `advance n` move forward `n` codepoints, if `n < 0`, move backward. Stops at beginning or end.

## Efficient pos stack manipulation

- `push_pos` pushes current pos into the stack.
- `pop_pos` sets current pos to top of the stack, and pops it.
- `drop_top` drops top of pos stack without changing current pos.
- `resume_top` sets current pos to top of the stack.
