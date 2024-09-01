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

constexpr struct Success {} success;

template<typename MessageType = std::string>
class BasicError {
public:
    constexpr BasicError() = default;

    constexpr BasicError(Success)
    {
    }

    constexpr BasicError(MessageType&& content) : content(std::forward<MessageType>(content))
    {
    }

    template<typename U>
    constexpr BasicError(U&& content) requires std::is_convertible_v<U, MessageType>
    : content(static_cast<MessageType>(std::forward<U>(content)))
    {
    }

    template<typename U, typename Internal>
    constexpr BasicError(U&& content, const Internal& internal,
            const std::string& infix = " because of ")
        requires std::is_convertible_v<Internal, std::string> and
                 std::is_convertible_v<std::string, MessageType> and
                 std::is_convertible_v<U, std::string>
    : content(std::string(content) + infix + std::string(internal))
    {
    }

    inline operator bool() const
    {
        return failed();
    }

    inline bool successful() const
    {
        return !content.has_value();
    }

    inline bool failed() const
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
    inline std::string report(const std::string& prefix = "Error: ",
                              F tostr = +utils::stringify<MessageType>) const
    {
        if (successful())
            return "SUCCESS";
        return prefix + tostr(get());
    }

private:
    std::optional<MessageType> content;
};

template<typename RelatedType = void, typename MessageType = std::string>
using Error = BasicError<MessageType>;


template<> inline std::string utils::stringify<std::error_code>(const std::error_code& ec)
{
    return ec.message() + " (" + utils::stringify(ec.value()) + ")";
}

class ErrorCode : public BasicError<std::error_code>
{
public:
    using BasicError<std::error_code>::BasicError;
    ErrorCode(std::errc errc) : BasicError<std::error_code>(std::make_error_code(errc))
    {}

    inline static ErrorCode from_current_errno()
    {
        return ErrorCode(static_cast<std::errc>(errno));
    }
};

}
