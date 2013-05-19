require "mkmf"

create_makefile 'zscan'

makefile = File.read 'Makefile'
lines = makefile.lines.map do |line|
  if line.start_with?('ORIG_SRCS =')
    line.sub /$/, ' pack/pack.c'
  elsif line.start_with?('OBJS =')
    line.sub /$/, ' pack/pack.o'
  else
    line
  end
end
headers = Dir.glob('**/*.h').join ' '
File.open 'Makefile', 'w' do |f|
  f.puts lines
  f.puts "\n$(OBJS): #{headers}"
end
