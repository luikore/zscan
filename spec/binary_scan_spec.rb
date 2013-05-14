require_relative "spec_helper"

describe 'ZScan binary scanning methods' do
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
  
  it "#scan_binary" do
    s = ZScan.binary_spec do
      int8        # once
      uint32_le 2 # little endian, twice
      double_be 1 # big endian, once
    end
    a = [-1, 2, 3, 4.0]
    z = ZScan.new(a.pack('cI<2G') + 'rest')
    b = z.scan_binary s
    assert_equal 'rest', z.rest
    assert_equal a, b
  end
end
