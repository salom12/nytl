// Copyright (c) 2017 nyorain
// Distributed under the Boost Software License, Version 1.0.
// See accompanying file LICENSE or copy at http://www.boost.org/LICENSE_1_0.txt

// Extremely lightweight header-only unit testing for C++.
// Use the TEST(name), EXPECT(expression, expected) and ERROR(expression, error) macros.
// Configuration useful for inline testing (define before including the file):
//  - TEST_NO_MAIN: don't include the main function that just executes all tests.
//  - TEST_NO_IMPL: don't include the implementation.

#pragma once

#include <string>
#include <cstring>
#include <typeinfo>
#include <vector>
#include <iostream>
#include <cmath>
#include <sstream>

namespace test {

// Utility
namespace {
	template<typename... T> constexpr void unused(T&&...) {}
	template<typename... T> using void_t = void;
}

/// Class used for printing objects to the debug output.
/// Provided implementations just return the object when it is printable
/// a string with type and address of the object.
/// The call operations retrieves something from the object that can be printed
/// ot an ostream (the object itself or an alternative descriptive string).
/// \tparam T The type of objects to print.
template<typename T, typename C = void>
struct Printable {
	static std::string call(const T&)
	{
		static auto txt = std::string("<not printable : ") + typeid(T).name() + std::string(">");
		return txt;
	}
};

/// Default printable specialization for types that can itself
template<typename T>
struct Printable<T, void_t<decltype(std::declval<std::ostream&>() << std::declval<T>())>> {
	static const auto& call(const T& obj) { return obj; }
};

/// Uses the Printable template to retrieve something printable to an ostream
/// from the given object.
template<typename T>
decltype(auto) printable(const T& obj) { return Printable<T>::call(obj); }

/// Static class that holds all test to be run.
class Testing {
public:
	/// Holds all the available info for a failed test.
	struct FailInfo {
		int line;
		const char* file;
	};

	/// Represents a unit to test.
	struct Unit {
		using FuncPtr = void(*)();
		std::string name;
		FuncPtr func;
	};

public:
	// Config variables
	// the ostream to output to. Must not be set to nullptr. Defaulted to std::cout
	static std::ostream* output;

	// The width of the failure separator. Defaulted to 50.
	static unsigned int separationWidth;

	// The char to use in the failure separator line. Defaulted to '-'
	static char failSeparator;

	// The indentation prefix. Defaulted to 4 spaces, one could e.g. use '\t' instead.
	static std::string indentation;

public:
	/// Called when a check expect fails.
	/// Will increase the current fail count and print a debug message for
	/// the given information.
	/// Should be only called inside of a testing unit.
	template<typename V, typename E>
	static inline void expectFailed(const FailInfo& info, const V& value, const E& expected);

	/// Called when a check error fails.
	/// Should be only called inside of a testing unit.
	/// \param error The name of the expected error type
	/// \param other nullptr if there was no other error, the type of the
	/// other error thrown otherwise.
	static inline void errorFailed(const FailInfo& info, const char* error, const char* other);

	/// Adds the given unit to the list of units to test.
	/// Always returns 0, useful for static calling.
	static inline int add(const Unit& unit);

	/// Tests all registered units.
	static inline unsigned int run();

	/// Tests the given function for exception E.
	/// Returns true if E is thrown, or sets the given string
	/// to a string representing the alternative exception, if any.
	template<typename E, typename F>
	static bool errorTest(const F& func, std::string& alternativeMsg);

	/// Outputs a separation line.
	static void separationLine();

protected:
	/// Returns a string for the given number of failed tests.
	static inline std::string failString(unsigned int failCount);

	/// Prints the error for an unexpected exception
	static inline void unexpectedException(const std::string& errorString);

	static std::vector<Unit> units;
	static unsigned int totalFailed;
	static unsigned int currentFailed;
	static const char* currentTest;
	static std::stringstream errout;
};

} // namespace test

/// Declares a new testing unit. After this macro the function body should follow like this:
/// ``` TEST(SampleTest) { EXPECT(1 + 1, 2); } ```
#define TEST(name) \
	static void TEST_##name##_U(); \
	namespace { static auto TEST_##name = test::Testing::add({#name, TEST_##name##_U}); } \
	static void TEST_##name##_U()

/// Expects the two given values to be equal.
#define EXPECT(expr, expected) { \
	auto&& TEST_exprValue = expr; \
	auto&& TEST_expectedValue = expected; \
	if(TEST_exprValue != TEST_expectedValue) \
		test::Testing::expectFailed({__LINE__, __FILE__}, TEST_exprValue, TEST_expectedValue); \
	}

/// Expects the given expression to throw an error of the given type when
/// evaluated.
#define ERROR(expr, error) { \
	std::string TEST_altMsg {}; \
	if(!test::Testing::errorTest<error>([&]{ expr; }, TEST_altMsg)) \
			test::Testing::errorFailed({__LINE__, __FILE__}, #error, TEST_altMsg.c_str()); \
	}

// Implementation
#ifndef TEST_NO_IMPL

namespace test {

unsigned int Testing::separationWidth = 50;
char Testing::failSeparator = '-';
std::string Testing::indentation = "    ";

std::vector<Testing::Unit> Testing::units {};
unsigned int Testing::totalFailed {};
unsigned int Testing::currentFailed {};
const char* Testing::currentTest {};
std::ostream* Testing::output = &std::cout;
std::stringstream Testing::errout {};

void Testing::separationLine()
{
	errout << indentation;
	for(auto i = 0u; i < separationWidth; ++i) errout << failSeparator;
	errout << "\n";
}

template<typename V, typename E>
void Testing::expectFailed(const FailInfo& info, const V& value, const E& expected)
{
	if(currentFailed == 0) separationLine();

	// error
	errout << indentation << "Check expect failed in test " << currentTest << "\n"
		   << indentation << info.file << ":" << info.line << "\n"
		   << indentation << "Expected " << printable(expected)
		   << ", got " << printable(value) << "\n";

	separationLine();
	++currentFailed;
}

void Testing::errorFailed(const FailInfo& info, const char* error, const char* other)
{
	// topline
	if(currentFailed == 0) separationLine();

	// error
	errout << indentation << "Check error failed in test " << currentTest << "\n"
		   << indentation << info.file << ":" << info.line << "\n"
		   << indentation << "Expected Error " << error << ", ";

	if(other) errout << "other error was thrown instead: " << other << "\n";
 	else errout << ", no error was thrown\n";

	separationLine();
	++currentFailed;
}

template<typename E, typename F>
bool Testing::errorTest(const F& func, std::string& msg)
{
	if constexpr(std::is_same<E, std::exception>::value) {
		try{
			func();
		} catch(const E&) {
			return true;
		} catch(...) {
			msg = "<Not a std::exception>";
		}
	} else {
		try{
			func();
		} catch(const E&) {
			return true;
		} catch(const std::exception& err) {
			msg = "std::exception: ";
			msg += err.what();
		} catch(...) {
			msg = "<Not a std::exception>";
		}
	}

	return false;
}

int Testing::add(const Unit& unit)
{
	units.push_back(unit);
	return 0;
}

unsigned int Testing::run()
{
	for(auto unit : units) {
		errout.str("");
		currentFailed = 0;

		currentTest = unit.name.c_str();
		auto thrown = false;

		try {
			unit.func();
		} catch(const std::exception& exception) {
			thrown = true;
			unexpectedException(std::string("std::exception::what(): ") + exception.what());
		} catch(...) {
			thrown = true;
			unexpectedException("<Not a std::exception object>");
		}

		auto fstr = failString(currentFailed);
		if(thrown) fstr += ", unexpected exception thrown!";

		auto errstr = errout.str();
		*output << unit.name << ": " << fstr << "\n";
		if(!errstr.empty()) *output << errstr << "\n";

		totalFailed += currentFailed;
	}

	*output << "Total" << ": " << failString(totalFailed) << "\n";
	return totalFailed;
}

std::string Testing::failString(unsigned int failCount)
{
	if(failCount == 0) {
		return "All tests succeeded!";
	} else if(failCount == 1) {
		return "1 test failed!";
	} else {
		return std::to_string(failCount) + " tests failed!";
	}
}

void Testing::unexpectedException(const std::string& errorString)
{
	if(currentFailed == 0) separationLine();

	errout << indentation << "Unexpected error in test " << currentTest << ":\n"
		   << indentation << errorString << "\n";

	separationLine();
	++currentFailed;
}

}

#endif // TEST_NO_IMPL

// Main function
#ifndef TEST_NO_MAIN

int main()
{
	return test::Testing::run();
}

#endif // TEST_NO_MAIN
