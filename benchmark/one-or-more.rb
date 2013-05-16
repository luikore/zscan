require_relative "../lib/zscan"
require "benchmark"

z = ZScan.new 'a' * 100
puts Benchmark.measure{
  1000.times{
    z.pos = 0
    z.one_or_more{
      z.scan 'a'
    }
  }
}

puts Benchmark.measure{
  1000.times{
    z.pos = 0
    z.scan /a+/
  }
}
