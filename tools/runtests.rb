template_file = File.open("./tests/test.cpp")
template = template_file.read
template_file.close

test_functions = template.scan(/(?<=void )test_[a-zA-Z0-9_]+/)

test_file = File.open("./test.cpp", "w")

test_file.write("// AUTO GENERATED FILE. DO NOT EDIT.\n\n")
test_file.write(template)
test_file.write("\nint main() {\n")
test_file.write(test_functions.map { |fn| "  #{fn}();\n" }.join);
test_file.write("  cout << endl;\n}\n");

test_file.close

system('make', 'clean')
system('make', 'runtests')
