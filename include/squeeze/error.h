#pragma once

#include <string_view>
#include <optional>
#include <utility>

#include "utils/stringify.h"

namespace squeeze {

namespace detail {

template<typename T>
struct OptionalField_Or_NoField;

template<> struct OptionalField_Or_NoField<void> {
    struct type {
        bool has_value()
        {
            return false;
        }
    };
};

template<typename T> struct OptionalField_Or_NoField {
    using type = std::optional<T>;
};

}

template<typename MessageType = std::string, typename CodeType = void>
class BasicError {
public:
    BasicError() = default;

    BasicError(MessageType&& message) : message(std::move(message))
    {}

    BasicError(MessageType&& message, CodeType&& code) : message(std::move(message)), code(std::move(code))
    {}

    template<typename MessageTypeOther = MessageType, typename CodeTypeOther = CodeType,
        typename = std::enable_if_t<std::is_convertible_v<MessageTypeOther, MessageType>>>
    BasicError(MessageTypeOther&& message) : message(std::forward<MessageTypeOther>(message))
    {}

    template<typename MessageTypeOther = MessageType, typename CodeTypeOther = CodeType,
        typename = std::enable_if_t<std::is_convertible_v<MessageTypeOther, MessageType> && std::is_convertible_v<CodeTypeOther, CodeType>>>
    BasicError(MessageTypeOther&& message, CodeTypeOther&& code)
        : message(std::forward<MessageTypeOther>(message)), code(std::forward<CodeTypeOther>(code))
    {}

    inline bool has_message()
    {
        return message.has_value();
    }

    inline bool has_code()
    {
        return code.has_value();
    }

    inline const MessageType& get_message() const
    {
        return *message;
    }

    inline MessageType& get_message()
    {
        return *message;
    }

    inline const CodeType& get_code() const
    {
        return *code;
    }

    inline CodeType& get_code()
    {
        return *code;
    }

    inline bool successful()
    {
        return !has_message() && !has_code();
    }

    inline bool failed()
    {
        return has_message() || has_code();
    }

    inline std::string report()
    {
        return "Error: " + (has_message() ? utils::stringify(get_message()) : "")
                         + (has_code() ? '(' + utils::stringify(get_code()) + ')' : "");
    }

private:
    typename detail::OptionalField_Or_NoField<MessageType>::type message;
    typename detail::OptionalField_Or_NoField<CodeType>::type code;
};


template<typename RelatedType, typename MessageType = std::string_view, typename CodeType = int>
using Error = BasicError<MessageType, CodeType>;

}
