#pragma once

#define NON_COPYABLE(NAME)                 \
public:                                    \
	NAME(const NAME&)            = delete; \
	NAME& operator=(const NAME&) = delete; \
                                           \
private:

#define NON_MOVABLE(NAME)                      \
public:                                        \
	NAME(NAME&&) noexcept            = delete; \
	NAME& operator=(NAME&&) noexcept = delete; \
                                               \
private:

// rule 5
#define NON_COPY_MOVABLE(NAME) NON_COPYABLE(NAME) NON_MOVABLE(NAME)
