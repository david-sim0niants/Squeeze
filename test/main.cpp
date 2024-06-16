#include <gtest/gtest.h>

#include <string_view>
#include <unordered_map>

#include "squeeze/logging.h"

using namespace std::string_view_literals;

int main(int argc, char *argv[])
{
    ::testing::InitGoogleTest(&argc, argv);

    squeeze::init_logging();
    if (argc >= 3) {
        if (argv[1] == "--log-level"sv || argv[1] == "-l"sv) {
            std::string log_level_str(argv[2]);

            for (char& c : log_level_str)
                c = std::toupper(c);

            using enum squeeze::LogLevel;
            static const std::unordered_map<std::string_view, squeeze::LogLevel> log_level_map =
            {
                { "TRACE",    Trace    },  { "0", Trace    },
                { "DEBUG",    Debug    },  { "1", Debug    },
                { "INFO",     Info     },  { "2", Info     },
                { "WARN",     Warn     },  { "3", Warn     },
                { "ERROR",    Error    },  { "4", Error    },
                { "CRITICAL", Critical },  { "5", Critical },
                { "OFF",      Off      },  { "6", Off      },
            };

            auto it = log_level_map.find(log_level_str);
            if (it == log_level_map.end())
                std::cerr << "Error: invalid log level - " << log_level_str << std::endl;
            squeeze::LogLevel log_level = it->second;
            squeeze::set_log_level(log_level);
        }
    }

    return RUN_ALL_TESTS();
}
