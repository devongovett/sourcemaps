{
  "targets": [
    {
      "target_name": "sourcemap",
      "defines": [ "NAPI_DISABLE_CPP_EXCEPTIONS" ],
      "sources": [ "src/binding.cc", "simdjson/singleheader/simdjson.cpp" ],
      "include_dirs" : ["<!@(node -p \"require('node-addon-api').include\")"],
      "dependencies": ["<!(node -p \"require('node-addon-api').gyp\")"],
      "cflags!": ["-fno-exceptions"],
      "cflags_cc!": ["-O3", "-fno-exceptions", "-std=gnu++0x", "-std=gnu++1y", "-mavx2", "-mavx", "-mbmi", "-mpclmul"],
      "cflags_cc+": ["-O3", "-march=native", "-std=c++17", "-mavx2", "-mavx", "-mbmi", "-mpclmul"],
      "xcode_settings": {
        "GCC_ENABLE_SSE42_EXTENSIONS": "YES",
        "CLANG_X86_VECTOR_INSTRUCTIONS": "avx2",
        "OTHER_CFLAGS": ["-mavx2", "-mavx", "-mbmi", "-mpclmul", "-std=c++17"],
        "GCC_ENABLE_CPP_EXCEPTIONS": "YES"
      },
      "msvs_settings": {
        "VCCLCompilerTool": {
          "AdditionalOptions": ["/std:c++17", "/arch:AVX2"]
        }
      }
    }
  ]
}
