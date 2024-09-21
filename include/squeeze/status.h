#pragma once

#include <cstdint>
#include <optional>
#include <ostream>
#include <sstream>
#include <memory>
#include <type_traits>
#include <variant>

#include "squeeze/utils/overloaded.h"
#include "printing.h"

namespace squeeze {

enum class Success { success };
using Success::success;

struct ErrorFormat {
    std::string_view prefix = "Error: ";
    std::string_view infix = " because of ";
    std::string_view postfix = "";
};

class BaseStatus {
public:
    BaseStatus() = default;
    virtual ~BaseStatus() = default;

    template<typename ReasonStatus>
    BaseStatus(ReasonStatus&& reason)
        requires std::is_base_of_v<BaseStatus, std::remove_cvref_t<ReasonStatus>> 
        : reason(std::make_unique<ReasonStatus>(std::move(reason)))
    {
    }

    template<typename ReasonStatus>
    BaseStatus(ReasonStatus& reason)
        requires std::is_base_of_v<BaseStatus, std::remove_cvref_t<ReasonStatus>> 
        : reason(std::make_unique<ReasonStatus>(std::move(reason)))
    {
    }

    BaseStatus(BaseStatus&&) = default;
    BaseStatus& operator=(BaseStatus&&) = default;

    BaseStatus(const BaseStatus&) = delete;
    BaseStatus& operator=(const BaseStatus&) = delete;

    virtual bool successful() const noexcept = 0;
    virtual bool failed() const noexcept = 0;

    inline operator bool() const noexcept
    {
        return successful();
    }

    virtual void print_to(std::ostream& os) const = 0;

    inline void report_to(std::ostream& os, const ErrorFormat& format = ErrorFormat()) const
    {
        if (successful())
            return squeeze::print_to(os, "SUCCESS");
        squeeze::print_to(os, format.prefix);
        print_to(os);
        if (reason) {
            squeeze::print_to(os, format.infix);
            reason->report_to(os);
        }
        squeeze::print_to(os, format.postfix);
    }

    inline std::string report(const ErrorFormat& format = ErrorFormat()) const
    {
        std::ostringstream oss;
        report_to(oss, format);
        return std::move(oss).str();
    }

    inline const BaseStatus *get_reason() const noexcept
    {
        return reason.get();
    }

private:
    std::unique_ptr<BaseStatus> reason;
};

template<typename MessageType>
struct IsSuccessful {
    inline bool operator()(const MessageType& message) const noexcept
    {
        return message == MessageType();
    }
};

template<typename MessageType,
    std::invocable<const MessageType&> IsSuccessful = IsSuccessful<MessageType>>
class Status : public BaseStatus {
public:
    Status() = default;

    Status(Success) : Status()
    {
    }

    template<typename FromMessageType>
    Status(FromMessageType&& message)
        requires std::is_convertible_v<FromMessageType, MessageType>
        :
        message(static_cast<MessageType>(std::forward<FromMessageType>(message)))
    {
    }

    template<typename FromMessageType, typename ReasonStatus>
    Status(FromMessageType&& message, ReasonStatus&& reason)
        requires std::is_convertible_v<FromMessageType, MessageType>
        :
        BaseStatus(std::forward<ReasonStatus>(reason)),
        message(static_cast<MessageType>(std::move(message)))
    {
    }

    constexpr inline bool successful() const noexcept override
    {
        return is_successful(message);
    }

    constexpr inline bool failed() const noexcept override
    {
        return not is_successful(message);
    }

    virtual void print_to(std::ostream& os) const override
    {
        squeeze::print_to(os, message);
    }

    constexpr inline MessageType& get() noexcept
    {
        return message;
    }

    constexpr inline const MessageType& get() const noexcept
    {
        return message;
    }

protected:
    MessageType message = MessageType();

    inline static IsSuccessful is_successful = IsSuccessful();
};

template<typename MessageType>
Status(MessageType) -> Status<MessageType>;

template<typename Content>
    requires requires (const Content& content) { {content.empty()} -> std::convertible_to<bool>; }
struct IsSuccessful<Content> {
    inline bool operator()(const Content& content) const noexcept
    {
        return content.empty();
    }
};

template<typename Message>
struct IsSuccessful<std::optional<Message>> {
    inline bool operator()(const std::optional<Message>& obj) const noexcept
    {
        return !obj.has_value();
    }
};

template<> struct IsSuccessful<std::error_code> {
    inline bool operator()(const std::error_code& ec) const noexcept
    {
        return !ec;
    }
};

template<typename... Ts> struct IsVariadicSuccessful {
    inline bool operator()(const std::variant<std::monostate, Ts...>& message) const noexcept
    {
        return std::holds_alternative<std::monostate>(message);
    }
};

template<typename... MessageTypes>
class VariadicStatus : public Status<std::variant<std::monostate, MessageTypes...>,
    IsVariadicSuccessful<MessageTypes...>> {
protected:
    using Base = Status<std::variant<std::monostate, MessageTypes...>, IsVariadicSuccessful<MessageTypes...>>;

public:
    using Base::Base;

    void print_to(std::ostream& os) const override
    {
        std::visit(utils::Overloaded {
                [&os] (std::monostate)
                {
                    squeeze::print_to(os, "SUCCESS");
                },
                [&os] (const auto& str)
                {
                    squeeze::print_to(os, str);
                },
            }, Base::message);
    }

};

using StatStr = VariadicStatus<const char *, std::string_view, std::string>;
using StatCode = Status<std::error_code>;

template<> inline void print_to(std::ostream& os, const std::error_code& ec)
{
    os << ec.message();
}

template<typename StatusType> requires std::is_base_of_v<BaseStatus, StatusType>
inline void print_to(std::ostream& os, const StatusType& status)
{
    status.report_to(os);
}

}
