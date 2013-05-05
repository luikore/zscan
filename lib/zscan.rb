require_relative "../ext/zscan"

class ZScan
  VERSION = '0.1'

  def initialize s, dup=false
    _internal_init dup ? s.dup : s
  end

  def string
    _internal_string.dup
  end

  def scan re_or_str
    if sz = bmatch?(re_or_str)
      r = _internal_string.byteslice bytepos, sz
      self.bytepos += sz
      r
    end
  end

  def skip re_or_str
    if sz = bmatch?(re_or_str)
      self.bytepos += sz
    end
  end

  def pos= new_pos
    advance new_pos - pos
  end

  def rest
    _internal_string.byteslice bytepos
  end

  private :_internal_init, :_internal_string
end

# coding: utf-8
if __FILE__ == $PROGRAM_NAME
  z = ZScan.new 'ab你好'
  z.push_pos
  z.scan /ab你/
  p z.pos
  p z.bytepos
  z.pop_pos
end
