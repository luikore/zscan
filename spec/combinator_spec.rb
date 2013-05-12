require_relative "spec_helper"

describe ZScan do
  it "#try restores pos" do
    z = ZScan.new "hello"
    return1 = z.try do
      z.scan 'h'
      z.scan 'e'
    end
    assert_equal 'e', return1
    assert_equal 2, z.pos

    return2 = z.try do
      z.scan 'l'
      z.scan 'l'
      z.scan 'p' # fails
    end
    assert_equal nil, return2
    assert_equal 2, z.pos
  end

  it "#zero_or_one" do
    z = Zscan.new "aab"
    assert_equal ['a'], z.zero_or_one{z.scan 'a'}
    assert_equal 1, z.pos

    z = Zscan.new 'aab'
    assert_equal [], z.zero_or_one{z.scan 'b'}
    assert_equal 0, z.pos
  end

  it "#zero_or_more" do
    z = Zscan.new "aab"
    assert_equal ['a', 'a'], z.zero_or_more{z.scan 'a'}
    assert_equal 2, z.pos

    assert_equal 'aab', z.zero_or_more('aa'){z.scan 'c'; z.scan 'b'}

    z = Zscan.new 'aab'
    assert_equal [], z.zero_or_more{z.scan 'b'}
    assert_equal 0, z.pos
  end

  it "#one_or_more" do
    z = Zscan.new 'aab'
    assert_equal ['a', 'a'], z.one_or_more{z.scan 'a'}
    assert_equal 2, z.pos

    z = Zscan.new 'aab'
    assert_equal nil, z.one_or_more([]){z.scan 'b'}
  end
end
