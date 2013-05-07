require "strscan"
require_relative "lib/zscan"
require "benchmark"

s = "word\n"
s *= 3_000_000

puts "StringScanner:"
sc = StringScanner.new s
puts Benchmark.measure{
  until sc.eos?
    sc.scan(/\w+/) and (sc.pos += 1)
  end
}

puts
puts "ZScan should be nearly as fast as StringScanner"
zc = ZScan.new s
puts Benchmark.measure{
  until zc.eos?
    zc.scan(/\w+/) and zc.advance(1)
  end
}
