is_reference <- function(x, y) .Call("rlang_is_reference", x, y)
is_missing <- function(x) missing(x) || is_reference(x, quote(expr = ))
f <- function(x) is_missing(x)

f()
f()
f()
f()
