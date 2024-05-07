#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>

std::string load_file(char const* fname)
{
	std::ifstream fp(fname, std::ios::in | std::ios::binary);
	if (!fp.good())
		return std::string();

	fp.seekg(0, std::ios::end);
	std::size_t fsize = fp.tellg();
	fp.seekg(0, std::ios::beg);

	std::string result;
	result.reserve(fsize + 1);
	result.assign(std::istreambuf_iterator<char>(fp), std::istreambuf_iterator<char>());

	return (result.size() == fsize) ? result : std::string();
}

char nibble_to_char(unsigned char chr)
{
	if (chr < 10)
		return chr + '0';
	else
		return chr + 'a' - 10;
}

std::string create_var_name(char const* suggestion)
{
	std::string result = suggestion;

	for (char& x : result)
	{
		x = std::tolower(x);
		if (x == '.' || x == '-' || x <= 32)
			x = '_';
	}

	return result;
}

int main(int argc, char** argv)
{
	if (argc < 4 || !*argv[1] || !*argv[2] || !*argv[3])
	{
		std::cout << "Missing argument(s)\n"
		          << "Syntax: " << argv[0] << " <input-file> <output-file> <variable-name>"
		          << std::endl;
		return EXIT_FAILURE;
	}

	std::string input_data = load_file(argv[1]);
	if (input_data.empty())
	{
		std::cerr << "Input file missing or no data" << std::endl;
		return EXIT_FAILURE;
	}

	std::string var_name = create_var_name(argv[3]);

	std::string new_output;
	new_output.reserve(512 + input_data.size() * 7);

	new_output  = "// This file is auto-generated during the build process based on the following input file:\n";
	new_output += "// ";
	new_output += argv[1];
	new_output += "\n\n";

	new_output += "#include <string_view>\n\n";
	new_output += "namespace\n{\n\n";
	new_output += "unsigned char _";
	new_output += var_name;
	new_output += "[] = {";

	std::string existing_output = load_file(argv[2]);
	if (!existing_output.empty())
	{
		// Check the header. Only overwrite if it matches with what we expect.
		if (std::memcmp(existing_output.data(), new_output.data(), new_output.size()) != 0)
		{
			std::cerr << "Output file exists and looks unexpected. Aborting." << std::endl;
			return EXIT_FAILURE;
		}
	}

	std::size_t const last_idx = input_data.size();
	for (std::size_t idx = 0; idx < last_idx; ++idx)
	{
		if (idx % 12 == 0)
			new_output += "\n  ";

		unsigned char const chr = input_data[idx];

		new_output += '0';
		new_output += 'x';
		new_output += nibble_to_char(chr >> 4);
		new_output += nibble_to_char(chr & 15);
		new_output += ',';
		new_output += ' ';
	}
	new_output += "\n};\n";

	new_output += "unsigned int _";
	new_output += var_name;
	new_output += "_len = ";
	new_output += std::to_string(last_idx);
	new_output += ";\n\n";

	new_output += "} // namespace\n\n";

	new_output += "namespace builtins\n{\n\n";
	new_output += "std::basic_string_view<unsigned char> ";
	new_output += var_name;
	new_output += "(_";
	new_output += var_name;
	new_output += ", _";
	new_output += var_name;
	new_output += "_len);\n\n";

	new_output += "} // namespace builtins\n";

	// Only update the file if it actually changed
	if (existing_output == new_output)
		return EXIT_SUCCESS;

	std::basic_fstream<char> fp(argv[2], std::ios::out | std::ios::binary | std::ios::trunc);
	if (!fp.good())
	{
		std::cerr << "Failed to open output file for writing" << std::endl;
		return EXIT_FAILURE;
	}

	fp.write(new_output.data(), new_output.size());
	fp.close();

	return EXIT_SUCCESS;
}
