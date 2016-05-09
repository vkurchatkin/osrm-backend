# Fuzz Testing

Fuzz testing using LLVM's libFuzzer.

## Building libFuzzer.a

Download and extract the free standing Fuzzer LLVm sub project:

    wget http://llvm.org/releases/3.8.0/llvm-3.8.0.src.tar.xz
    tar xf llvm-3.8.0.src.tar.xz llvm-3.8.0.src/lib/Fuzzer

Use the same compiler and stdlib you want to compile your project with to generate the `libFuzzer` object files.

For example on Linux, `gcc`, `stdlibc++`:

    g++ -std=c++11 -c -g -O2 llvm-3.8.0.src/lib/Fuzzer/*.cpp -Illvm-3.8.0.src/lib/Fuzzer/

Or with `clang`, `libc++`:

    clang++ -std=c++11 -stdlib=libc++ -c -g -O2 ~/llvm/lib/Fuzzer/*.cpp -I~/llvm/lib/Fuzzer
    
Bundle `libFuzzer` object files into static library `libFuzzer.a`:

    ar ruv libFuzzer.a Fuzzer*.o

Now put `libFuzzer.a` into `/usr/lib` (or `/usr/local/lib` or specify the path in `ld.conf`), and update the cache with `ldconfig`.

## Build libosrm Fuzzers

    cmake .. -DENABLE_FUZZING=ON -DFUZZ_SANITIZER=undefined,address

## Run libosrm Fuzzers

    make fuzz-escape_json

Targets for fuzzers are `fuzz-` prefixed.

## Adding your own Fuzzer

- Add a driver to the `fuzz/` directory
- Write a function `extern "C" int LLVMFuzzerTestOneInput(const unsigned char *data, unsigned long size);`
- Add the driver to `fuzz/CMakeLists.txt`

## References

- http://llvm.org/docs/LibFuzzer.html
- http://llvm.org/releases/3.8.0/docs/LibFuzzer.html
