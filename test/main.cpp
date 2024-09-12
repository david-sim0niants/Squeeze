#include <gtest/gtest.h>

#include <string_view>
#include <cstring>
#include <charconv>
#include <unordered_map>
#include <fstream>

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
                { "TRACE",    Trace    }, { "T", Trace    }, { "0", Trace    },
                { "DEBUG",    Debug    }, { "D", Debug    }, { "1", Debug    },
                { "INFO",     Info     }, { "I", Info     }, { "2", Info     },
                { "WARN",     Warn     }, { "W", Warn     }, { "3", Warn     },
                { "ERROR",    Error    }, { "E", Error    }, { "4", Error    },
                { "CRITICAL", Critical }, { "C", Critical }, { "5", Critical },
                { "OFF",      Off      }, { "O", Off      }, { "6", Off      },
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
