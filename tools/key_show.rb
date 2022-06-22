def read_char
  system('stty', 'raw', '-echo')
  char = STDIN.getc
  system('stty', '-raw', 'echo')
  char
end

loop do
  c = read_char
  break if c == 'q'

  puts("#{c.ord} <#{c.ord >= 32 ? c : '-'}>")
end

puts
