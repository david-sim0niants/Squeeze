#define DEFER_INTERNAL_CONCAT_IMPL(x, y) x##y
#define DEFER_INTERNAL_CONCAT(x, y) DEFER_INTERNAL_CONCAT_IMPL(x, y)
#define DEFER(f) ::squeeze::utils::Defer DEFER_INTERNAL_CONCAT(_defer_, __LINE__) (f)
