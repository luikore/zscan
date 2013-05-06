## Features

- `ZScan#scan`/`ZScan#skip`/`ZScan#match_bytesize` accept either string or regexp as param.
- `ZScan#pos` is the codepoint position, and `ZScan#bytepos` is byte position.
- Correctly scans anchors and look behind predicates.
- Pos stack manipulation.

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

## Motivation

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

## Methods

- `ZScan.new string, dup=false`
- `#scan regexp_or_string`
- `#skip regexp_or_string`
- `#match_bytesize regexp_or_string` returns length of matched bytes or nil
- `#eos?`
- `#string` note: returns a COW dup
- `#rest`

## Pos management

- `#pos`
- `#pos= new_pos` note: complexity ~ `new_pos > pos ? new_pos - pos : new_pos`.
- `#bytepos`
- `#bytepos= new_bytepos` note: complexity ~ `abs(new_bytepos - bytepos)`.
- `#advance n` move forward `n` codepoints, if `n < 0`, move backward. Stops at beginning or end.
- `#reset` go to beginning.
- `#terminate` go to end of string.

## Efficient pos stack manipulation

- `#push` pushes current pos into the stack.
- `#pop` sets current pos to top of the stack, and pops it.
- `#drop` drops top of pos stack without changing current pos.
- `#restore` sets current pos to top of the stack.

## License

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
