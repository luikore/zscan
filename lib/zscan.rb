require_relative "../ext/zscan"

class ZScan
  VERSION = '1.0.1'

  def initialize s, dup=false
    _internal_init dup ? s.dup : s
  end

  def string
    _internal_string.dup
  end

  def skip re_or_str
    if sz = match_bytesize(re_or_str)
      self.bytepos += sz
    end
  end

  def pos= new_pos
    advance new_pos - pos
  end

  def rest
    _internal_string.byteslice bytepos, _internal_string.bytesize
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

  private :_internal_init, :_internal_string
end

# rooooobust!
Zscan = ZScan
