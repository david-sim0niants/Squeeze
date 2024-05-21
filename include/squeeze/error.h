#pragma once

#include <string>
#include <optional>
#include <utility>
#include <system_error>

#include "utils/stringify.h"

namespace squeeze {

namespace detail {

template<typename T>
struct OptionalField_Or_NoField;

template<typename T> struct OptionalField_Or_NoField {
    using type = std::optional<T>;
};

template<> struct OptionalField_Or_NoField<void> {
    struct type {
        void operator*(){}
        const void operator*() const {}

        bool has_value()
        {
            return false;
        }
    };
};

}

template<typename MessageType = std::string>
class BasicError {
public:
    BasicError() = default;

    BasicError(MessageType&& content) : content(std::forward<MessageType>(content))
    {}

    template<typename U>
    BasicError(U&& content) requires std::is_convertible_v<U, MessageType>
    : content(static_cast<MessageType>(std::forward<U>(content)))
    {}

    inline bool successful()
    {
        return !content.has_value();
    }

    inline bool failed()
    {
        return content.has_value();
    }

    inline MessageType& get()
    {
        return *content;
    }

    inline const MessageType& get() const
    {
        return *content;
    }

    template<typename F = decltype(utils::stringify<MessageType>),
        typename = std::enable_if_t<std::is_invocable_v<F, MessageType>>>
    inline std::string report(std::string&& prefix = "Error", F tostr = +utils::stringify<MessageType>)
    {
        if (successful())
            return "SUCCESS";
        return prefix + tostr(get());
    }

private:
    std::optional<MessageType> content;
};

template<typename RelatedType, typename MessageType = std::string>
using Error = BasicError<MessageType>;

template<> inline std::string utils::stringify<std::error_code>(const std::error_code& ec)
{
    return ec.message() + " (" + utils::stringify(ec.value()) + ")";
}

using ErrorCode = BasicError<std::error_code>;

}
