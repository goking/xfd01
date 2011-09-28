if ARGV.size != 1
  STDERR.puts("usage: ruby ipaddr.rb ipaddr")
  exit 1
end
ipaddr = ARGV.shift.split('.')
data = ipaddr.map {|i| format("%02x", i.to_i)}.join.to_a.pack("H*")
print(data)
