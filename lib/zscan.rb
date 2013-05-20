require_relative "../ext/zscan"
require_relative "zscan/bspec"
require "date"

class ZScan
  VERSION = '1.3'

  def initialize s, dup=false
    if s.encoding.ascii_compatible?
      s = dup ? s.dup : s
    else
      s = s.encode 'utf-8'
    end
    _internal_init s
  end

  def string
    _internal_string.dup
  end

  def skip re_or_str
    if sz = match_bytesize(re_or_str)
      self.bytepos += sz
    end
  end

  def scan_int radix=nil
    negative = false
    r = try do
      negative = (scan(/[+\-]/) == '-')
      if radix.nil?
        radix =
          if scan(/0b/i)
            2
          elsif scan(/0x/i)
            16
          elsif scan('0')
            8
          else
            10
          end
      end
      scan \
        case radix
        when 2;  /[01]+/
        when 8;  /[0-7]+/
        when 10; /\d+/
        when 16; /\h+/i
        else
          if radix < 10
            /[0-#{radix}]+/
          elsif radix > 36
            raise ArgumentError, "invalid radix #{radix}"
          else
            end_char = ('a'.ord + (radix - 11)).chr
            /[\da-#{end_char}]+/i
          end
        end
    end
    if r
      r = r.to_i radix
      negative ? -r : r
    end
  end

  def scan_date format, start=Date::ITALY
    s = rest
    d = DateTime._strptime s, format
    if d
      # XXX need 2 parses because the handling is very complex ...
      dt = DateTime.strptime s, format, start rescue return nil

      len = s.bytesize
      if leftover = d[:leftover]
        len -= leftover.bytesize
      end
      self.bytepos += len

      dt
    end
  end

  def pos= new_pos
    advance new_pos - pos
  end

  def reset
    self.pos = 0
    self
  end

  def terminate
    self.pos = _internal_string.size
  end

  def << substring
    _internal_string << substring
  end

  def []= range, substring
    start = range.start
    if start < 0
      start = _internal_string.size + start
    end
    if start < pos
      self.pos = start
    end
    _internal_string[range] = substring
  end

  def size
    _internal_string.size
  end

  def bytesize
    _internal_string.bytesize
  end

  def line_index
    _internal_string.byteslice(0, bytepos).count "\n"
  end

  def slice size
    r = _internal_string.slice pos, size
    advance size
    r
  end

  def byteslice bytesize
    r = _internal_string.byteslice bytepos, bytesize
    self.bytepos += bytesize
    r
  end

  private :_internal_init, :_internal_string
end

# rooooobust!
Zscan = ZScan
