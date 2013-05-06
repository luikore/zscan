require_relative "../ext/zscan"

class ZScan
  VERSION = '0.5'

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
    _internal_string.byteslice bytepos, _internal_string.bytesize
  end

  private :_internal_init, :_internal_string
end
