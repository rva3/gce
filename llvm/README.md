Greetings,

We're happy to publish our LLVM fork and tooling for GCE v3 ISA. Thanks to Mr. Paxton who provided
us financial support for vibeporting infrastructure.

- You will need to clone LLVM separately. Use `git clone https://github.com/rva3/llvm-project`
- Prepare:
```
cmake -S llvm-project/llvm -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DLLVM_TARGETS_TO_BUILD="" \
        -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=GCE \
        -DLLVM_ENABLE_PROJECTS="clang;lld" \
        -DLLVM_ENABLE_ASSERTIONS=ON \
        -DCLANG_ENABLE_STATIC_ANALYZER=OFF
```

- Build:
```
cmake --build build --parallel 16 --target \
        clang lld llc llvm-ar llvm-mc llvm-objcopy llvm-readobj llvm-objdump
```

- Enjoy
