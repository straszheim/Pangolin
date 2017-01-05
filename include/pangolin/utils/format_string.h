#pragma once

#include <sstream>

namespace pangolin {
namespace details {

// Following: http://stackoverflow.com/a/22759544
template <typename T>
class IsStreamable {
private:
    template <typename TT>
    static auto test(int) -> decltype(std::declval<std::stringstream&>() << std::declval<TT>(), std::true_type());

    template <typename>
    static auto test(...) -> std::false_type;

public:
    static const bool value = decltype(test<T>(0))::value;
};

inline void FormatStream(std::stringstream& stream, const char* text)
{
    stream << text;
}

// Following: http://en.cppreference.com/w/cpp/language/parameter_pack
template <typename T, typename... Args>
void FormatStream(std::stringstream& stream, const char* text, T arg, Args... args)
{
    static_assert(IsStreamable<T>::value,
                  "One of the args has not an ostream overload!");
    for (; *text != '\0'; ++text) {
        if (*text == '%') {
            stream << arg;
            FormatStream(stream, text + 1, args...);
            return;
        }
        stream << *text;
    }
    stream << "\nFormat-Warning: There are " << sizeof...(Args) + 1
           << " args unused.";
}

}  // namespace details

template <typename... Args>
std::string FormatString(const char* text, Args... args)
{
    std::stringstream stream;
    details::FormatStream(stream, text, args...);
    return stream.str();
}

inline std::string FormatString()
{
    return std::string();
}

}
