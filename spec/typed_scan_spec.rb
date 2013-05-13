require_relative "spec_helper"

describe "typed scan" do
  it "#scan_int" do
    z = Zscan.new " 1 0b10F5 10 030"
    assert_equal nil, z.scan_int
    z.advance 1
    assert_equal 1, z.scan_int(10)

    z.advance 1
    assert_equal 0b10, z.scan_int
    assert_equal 0xF5, z.scan_int(16)

    z.advance 1
    assert_equal 12, z.scan_int(12)

    z.advance 1
    assert_equal 030, z.scan_int
  end

  it "#scan_float" do
    z = Zscan.new " -3.5e23"
    assert_equal nil, z.scan_float
    z.advance 1
    assert_equal -3.5e23, z.scan_float
  end
end
