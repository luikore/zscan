# coding: utf-8
require_relative "spec_helper"

describe 'ZScan binary scanning methods' do
  it "#unpack" do
    z = ZScan.new "\x01\x02\x03"
    assert_equal [1, 2], (z.unpack 'CC')
    assert_equal 2, z.pos
    assert_equal [nil], (z.unpack 'I')
    assert_equal 2, z.pos
  end

  it "#unpack position-changing instructions and var-length instructions" do
    z = ZScan.new "abcd\0abc"
    s, _ = z.unpack 'Z*'
    assert_equal "abcd", s
    assert_equal 5, z.pos

    z.reset
    s, _ = z.unpack '@2Z*'
    assert_equal 'cd', s
  end

  it "#scan_bytes" do
    s = ZScan::BSpec.new do
      int8
      2.times{ uint32_le } # little endian
      double_be            # big endian
      single
    end

    a = [-1, 2, 3, 4.0, 3.0]
    z = ZScan.new(a.pack('cI<2Gf') + 'rest')
    b = z.scan_bytes s
    assert_equal 'rest', z.rest
    assert_equal a, b
  end

  it "#scan_bytes with expectation" do
    s = ZScan::BSpec.new do
      int8 expect: 3
      float
    end

    a = [3, 4.0]
    z = ZScan.new a.pack('cf')
    assert_equal a, z.scan_bytes(s)

    a = [2, 4.0]
    z = ZScan.new a.pack('cf')
    assert_equal nil, z.scan_bytes(s)
  end
end
