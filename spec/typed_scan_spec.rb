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

  it "won't overflow in #scan_float" do
    s = '1.23E15'.byteslice 0, 4
    z = Zscan.new s
    assert_equal 1.23, z.scan_float
    assert_equal 4, z.pos
  end

  it "#scan_date" do
    z = Zscan.new " 2001 04 6 04 05 06 +7 231rest"
    assert_equal nil, z.scan_date('%Y %U %w %H %M %S %z %N')
    z.advance 1

    d = z.scan_date '%Y %U %w %H %M %S %z %N'
    assert_equal 0.231, d.sec_fraction
    assert_equal 'rest', z.rest

    z.pos = 1
    z.scan_date '%Y %U %w ahoy %H %M %S %z' # bad format
    assert_equal 1, z.pos
  end
end
