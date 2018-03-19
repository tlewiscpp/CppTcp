#ifndef PROJECTTEMPLATE_APPLICATIONUTILITIES_H
#define PROJECTTEMPLATE_APPLICATIONUTILITIES_H

#include <string>
#include <regex>
#include <vector>
#include <sstream>
#include <utility>
#include <tuple>
struct option;

namespace ApplicationUtilities
{
void installSignalHandlers(void (*signalHandler)(int));
std::string TStringFormat(const char *formatting);
std::string getTempDirectory();
std::string getLogFilePath();

    std::string buildShortOptions(option *longOptions, size_t numberOfLongOptions);

int removeFile(const std::string &filePath);
int createDirectory(const std::string &filePath, mode_t permissions = 0777);


std::string currentTime();
std::string currentDate();

bool endsWith(const std::string &str, const std::string &ending);
bool endsWith(const std::string &str, char ending);

template <typename T> static inline std::string toStdString(T t) { return dynamic_cast<std::ostringstream &>(std::ostringstream{} << t).str(); }

template <char Delimiter>
std::vector<std::string> split(const std::string &str)
{
    std::string tempString{""};
    std::vector<std::string> returnVector{};
    std::stringstream istr{str};
    for(std::string s{""}; std::getline(istr, s, Delimiter); ( (s.length() > 0) ?  returnVector.push_back(s) : (void)returnVector.size() ));
    return returnVector;
}


std::vector<std::string> split(std::string str, const std::string &delimiter);


/*Base case to break recursion*/
std::string TStringFormat(const char *formatting);

/*C# style String.Format()*/
template <typename First, typename ... Args>
std::string TStringFormat(const char *formatting, First &&first, Args&& ... args)
{
    /* Match exactly one opening brace, one or more numeric digit,
    * then exactly one closing brace, identifying a token */
    static const std::regex targetRegex{R"(\{[0-9]+\})"};
    std::smatch match;

    /* Copy the formatting string to a std::string, to
    * make for easier processing, which will eventually
    * be used (the .c_str() method) to pass the remainder
    * of the formatting recursively */
    std::string returnString{formatting};

    /* Copy the formatting string to another std::string, which
    * will get modified in the regex matching loop, to remove the
    * current match from the string and find the next match */
    std::string copyString{formatting};

    /* std::tuple to hold the current smallest valued brace token,
    * wrapped in a std::vector because there can be multiple brace
    * tokens with the same value. For example, in the following format string:
    * "There were {0} books found matching the title {1}, {0}/{2}",
    * this pass will save the locations of the first and second {0} */
    using TokenInformation = std::tuple<int, size_t, size_t>;
    std::vector<TokenInformation> smallestValueInformation{std::make_tuple(-1, 0, 0)};

    /*Iterate through string, finding position and lengths of all matches {x}*/
    while(std::regex_search(copyString, match, targetRegex)) {
        /*Get the absolute position of the match in the original return string*/
        size_t foundPosition{match.position() + (returnString.length() - copyString.length())};
        int regexMatchNumericValue{0};
        /*Convert the integer value between the opening and closing braces to an int to compare */
        regexMatchNumericValue = std::stoi(returnString.substr(foundPosition + 1, (foundPosition + match.str().length())));

        /*Do not allow negative numbers, although this should never get picked up the regex anyway*/
        if (regexMatchNumericValue < 0) {
            throw std::runtime_error(TStringFormat("ERROR: In TStringFormat() - Formatted string is invalid (formatting = {0})", formatting));
        }
        /* If the numeric value in the curly brace token is smaller than
        * the current smallest (or if the smallest value has not yet been set,
        * ie it is the first match), set the corresponding smallestX variables
        * and wrap them up into a TokenInformation and add it to the std::vector */
        int smallestValue{std::get<0>(smallestValueInformation.at(0))};
        if ((smallestValue == -1) || (regexMatchNumericValue < smallestValue)) {
            smallestValueInformation.clear();
            smallestValueInformation.push_back(std::make_tuple(regexMatchNumericValue,
                                                               foundPosition,
                                                               match.str().length()));
        } else if (regexMatchNumericValue == smallestValue) {
            smallestValueInformation.push_back(std::make_tuple(regexMatchNumericValue,
                                                               foundPosition,
                                                               match.str().length()));
        }
        copyString = match.suffix();
    }
    int smallestValue{std::get<0>(smallestValueInformation.at(0))};
    if (smallestValue == -1) {
        throw std::runtime_error(TStringFormat("ERROR: In TStringFormat() - Formatted string is invalid (formatting = {0})", formatting));
    }
    /* Set the returnString to be up to the brace token, then the string
    * representation of current argument in line (first), then the remainder
    * of the format string, effectively removing the token and replacing it
    * with the requested item in the final string, then pass it off recursively */

    std::string firstString{toStdString(first)};
    int index{0};
    for (const auto &it : smallestValueInformation) {
        size_t smallestValueLength{std::get<2>(it)};

        /* Since the original string will be modified, the adjusted position must be
        calculated for any repeated brace tokens, kept track of by index.
        The length of string representation of first mutiplied by which the iterationn count
        is added, and the length of the brace token multiplied by the iteration count is
        subtracted, resulting in the correct starting position of the current brace token */
        size_t lengthOfTokenBracesRemoved{index * smallestValueLength};
        size_t lengthOfStringAdded{index * firstString.length()};
        size_t smallestValueAdjustedPosition{std::get<1>(it) + lengthOfStringAdded - lengthOfTokenBracesRemoved};
        returnString = returnString.substr(0, smallestValueAdjustedPosition)
                       + firstString
                       + returnString.substr(smallestValueAdjustedPosition + smallestValueLength);
        index++;
    }
    return TStringFormat(returnString.c_str(), std::forward<Args>(args)...);
}


};


#endif //PROJECTTEMPLATE_APPLICATIONUTILITIES_H
