#include "stringutils.hpp"

bool matchPattern(std::string pattern, std::string tested)
{
	std::regex patternRegex(pattern);
	return regex_match(tested, patternRegex);
}
