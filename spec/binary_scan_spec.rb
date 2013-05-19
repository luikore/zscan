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

  it "#scan_binary" do
    s = ZScan::BinarySpec.new do
      int8        # once
      uint32_le 2 # little endian, twice
      double_be 1 # big endian, once
      single 1
    end

    a = [-1, 2, 3, 4.0, 3.0]
    z = ZScan.new(a.pack('cI<2Gf') + 'rest')
    b = z.scan_binary s
    assert_equal 'rest', z.rest
    assert_equal a, b
  end
end
