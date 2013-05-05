require_relative "../ext/zscan"

class ZScan
  VERSION = '0.4'

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

# coding: utf-8
if __FILE__ == $PROGRAM_NAME
  GC.stress = true
  def assert a, b
    raise "expected #{a.inspect} == #{b.inspect}" if a != b
  end
  z = ZScan.new 'ab你好'
  assert 2, z.bmatch?('ab')
  z.pos = 4
  assert 8, z.bytepos
  z.push_pos
  assert nil, z.scan(/ab你/)
  z.pos = 0
  assert 'ab你', z.scan(/ab你/)
  assert 3, z.pos
  assert 5, z.bytepos
  z.pop_pos
  assert 4, z.pos
  assert 8, z.bytepos
  z.bytepos = 2
  assert '你', z.scan('你')
  assert '好', z.rest

  z.bytepos = 20
  assert 8, z.bytepos

  z.skip('ab')
  assert 8, z.bytepos
  assert 4, z.pos

  z = ZScan.new "a x:b+ $ \\k<x>"
  z.pos = 1
  assert ' ', z.scan(/\s*(\#.*$\s*)*/)
end
