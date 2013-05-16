require "mkmf"

create_makefile 'zscan'

headers = Dir.glob('*.h').join ' '
File.open 'Makefile', 'a' do |f|
  f.puts "\n$(OBJS): #{headers}"
end
