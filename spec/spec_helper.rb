require_relative "../lib/zscan"
require 'rspec/autorun'
RSpec.configure do |config|
  config.expect_with :stdlib
  config.before :all do
    # GC.stress = true
  end
end
