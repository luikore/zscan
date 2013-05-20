require_relative "spec_helper"

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

  it "slices a certain bytes or chars" do
    assert_equal 'ab', (@z.slice 2)
    assert_equal '你', (@z.slice 1)
    assert_equal '好', (@z.byteslice 3)
    assert_equal true, @z.eos?
  end

  it "scans from middle" do
    @z.bytepos = 2
    assert_equal '你', @z.scan('你')
    assert_equal '好', @z.rest
    assert_equal '好'.size, @z.rest_size
    assert_equal '好'.bytesize, @z.rest_bytesize
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

  it "stack doesn't underflow" do
    @z.push
    @z.pop
    @z.pop
    @z.pos = 3
    @z.push
    @z.pos = 4
    @z.pop
    assert_equal 3, @z.pos
  end

  it "#reset, #terminate and #line_index" do
    z = ZScan.new ''
    assert_equal 0, z.line_index
    z.terminate
    assert_equal 0, z.line_index
    z.reset
    assert_equal 0, z.line_index

    z = ZScan.new "a\nb\nc"
    assert_equal 0, z.line_index
    z.terminate
    assert_equal 2, z.line_index
    z.reset
    assert_equal 0, z.line_index
    z.pos = 1
    assert_equal 0, z.line_index
    z.pos = 2
    assert_equal 1, z.line_index
  end

  it '#cleanup' do
    @z.scan /\w/
    @z.cleanup
    assert_equal 'b你好', @z.string
    assert_equal 0, @z.pos
    assert_equal 0, @z.bytepos
  end
end
