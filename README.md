# sofun - shared object function caller

Uses [dlsym(3)](https://linux.die.net/man/3/dlsym) to get function
pointers from shared object files, then uses 
[libffi](https://sourceware.org/libffi/) to infer function signature
at runtime.

WIP.
