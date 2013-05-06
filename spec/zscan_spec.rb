require_relative "../lib/zscan"
require 'rspec/autorun'
RSpec.configure do |config|
  config.expect_with :stdlib
end

# GC.stress = true

describe ZScan do
  before :each do
    @z = ZScan.new 'ab你好'
  end

  it "random workflow" do
    assert_equal 2, @z.match_bytesize('ab')
    @z.pos = 4
    assert_equal 8, @z.bytepos
    @z.push
    assert_equal nil, @z.scan(/ab你/)
    @z.pos = 0
    assert_equal 'ab你', @z.scan(/ab你/)

    @z.restore
    assert_equal 8, @z.bytepos
    @z.pos = 3
    @z.restore
    assert_equal 8, @z.bytepos
  end

  it "scans from middle" do
    @z.bytepos = 2
    assert_equal '你', @z.scan('你')
    assert_equal '好', @z.rest
  end

  it "won't overflow pos" do
    @z.pos = 20
    assert_equal 8, @z.bytepos
    assert_equal 4, @z.pos

    @z.skip('ab')
    assert_equal 8, @z.bytepos

    @z.pos = -1
    assert_equal 0, @z.bytepos
    assert_equal 0, @z.pos

    @z.bytepos = 20
    assert_equal 8, @z.bytepos
    assert_equal 4, @z.pos

    @z.bytepos = -1
    assert_equal 0, @z.bytepos
    assert_equal 0, @z.pos
  end

  it "recognizes anchors" do
    z = ZScan.new "a x:b+ $ \\k<x>"
    z.pos = 1
    assert_equal ' ', z.scan(/\s*(\#.*$\s*)*/)
    z.pos = 1
    assert_equal '', z.scan(/(?<=a)/)
    assert_equal nil, z.scan(/^/)
  end
end
