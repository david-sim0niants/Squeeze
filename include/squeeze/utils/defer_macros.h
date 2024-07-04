#define DEFER_INTERNAL_CONCAT_IMPL(x, y) x##y
#define DEFER_INTERNAL_CONCAT(x, y) DEFER_INTERNAL_CONCAT_IMPL(x, y)
#define DEFER_CALL(f) ::squeeze::utils::Defer DEFER_INTERNAL_CONCAT(_deferred_call_handle_, __LINE__) (f)
#define DEFER(...) DEFER_CALL([&](){ __VA_ARGS__; })
