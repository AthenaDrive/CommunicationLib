
#ifndef COMPUTERCONTROLL_RUNTIMEERROR_HPP
#define COMPUTERCONTROLL_RUNTIMEERROR_HPP

#include <source_location>
#include <stdexcept>
#include <string>

// RuntimeError made by ChatGPT
class RuntimeError : public std::runtime_error {
public:
	RuntimeError(
		const std::string& msg,
		std::source_location loc = std::source_location::current())
		: std::runtime_error(
			msg +
			"\nFile: " + loc.file_name() +
			"\nLine: " + std::to_string(loc.line()) +
			"\nFunction: " + loc.function_name())
	{}
};

#endif //COMPUTERCONTROLL_RUNTIMEERROR_HPP