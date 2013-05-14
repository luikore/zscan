require_relative "spec_helper"

describe ZScan do
  it "#unpack" do
    z = ZScan.new "\x01\x02\x03"
    assert_raise ArgumentError do
      z.unpack '@1C'
    end
    assert_equal [1, 2], (z.unpack 'CC')
    assert_equal 2, z.pos
    assert_equal nil, (z.unpack 'I')
    assert_equal 2, z.pos
  end
end
