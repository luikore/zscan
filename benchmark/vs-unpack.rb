require_relative "../lib/zscan"
require "benchmark"

spec = ZScan.binary_spec do
  int8
  double_le 2
  single_be
end

arr = [1, 1.1, 1.2, 1.3]
str = arr.pack 'cE2g'
z = Zscan.new str.b

puts 'reference nop group'
puts Benchmark.measure{ 100000.times{ z.pos = 0 } }
puts 'ZScan#unpack'
puts Benchmark.measure{ 100000.times{ z.pos = 0; z.unpack 'cE2g' } }
puts 'ZScan#scan_binary'
puts Benchmark.measure{ 100000.times{ z.pos = 0; z.scan_binary spec } }
puts 'String#unpack'
puts Benchmark.measure{ 100000.times{ z.pos = 0; str.unpack 'cE2g' } }
