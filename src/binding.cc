#include <napi.h>
#include <node_api.h>
#include <unordered_map>
#include <string>
#include <vector>
#include "../simdjson/singleheader/simdjson.h"

using namespace Napi;
using namespace simdjson;

struct Position {
  unsigned int line;
  unsigned int column;
};

struct Mapping {
  Position generated;
  bool hasOriginal;
  Position original;
  bool hasSource;
  std::string source;
  bool hasName;
  std::string name;
};

#define BASE64_MAP "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"
#define VLQ_BASE_SHIFT 5
#define VLQ_BASE (1 << VLQ_BASE_SHIFT)
#define VLQ_BASE_MASK (VLQ_BASE - 1)
#define VLQ_CONTINUATION_BIT VLQ_BASE

inline int toVLQSigned(const int aValue) {
  return (aValue < 0) ? ((-aValue) << 1) + 1 : (aValue << 1) + 0;
}

inline void base64VLQ_encode(const int aValue, std::ostream &os) {
  int vlq = toVLQSigned(aValue);
  do {
    // printf("vlq = %d\n", vlq);
    int digit = vlq & VLQ_BASE_MASK;
    vlq >>= VLQ_BASE_SHIFT;
    if (vlq > 0) {
      // There are still more digits in this value, so we must make sure the
      // continuation bit is marked.
      digit |= VLQ_CONTINUATION_BIT;
    }
    os.put(BASE64_MAP[digit]);
  } while (vlq > 0);
}

inline int compareByGeneratedPositionsInflated(Mapping &a, Mapping &b) {
  int cmp = a.generated.line - b.generated.line;
  if (cmp != 0) {
    return cmp;
  }

  cmp = a.generated.column - b.generated.column;
  if (cmp != 0) {
    return cmp;
  }

  if (a.hasSource && b.hasSource) {
    cmp = a.source.compare(b.source);
    if (cmp != 0) {
      return cmp;
    }
  }

  if (a.hasOriginal && b.hasOriginal) {
    cmp = a.original.line - b.original.line;
    if (cmp != 0) {
      return cmp;
    }

    cmp = a.original.column - b.original.column;
    if (cmp != 0) {
      return cmp;
    }
  }

  if (a.hasName && b.hasName) {
    cmp = a.name.compare(b.name);
    if (cmp != 0) {
      return cmp;
    }
  }

  return 0;
}

class SourceMap : public ObjectWrap<SourceMap> {
public:
  static Object Init(Napi::Env env, Object exports);

  SourceMap(const CallbackInfo& info) : Napi::ObjectWrap<SourceMap>(info) {
    Napi::Env env = info.Env();
    HandleScope scope(env);

    pj.allocate_capacity(1024 * 1024);
  }

private:
  static FunctionReference constructor;
  ParsedJson pj;
  std::vector<Mapping> mappings;
  std::unordered_map<std::string, int> sources;
  std::unordered_map<std::string, std::string> sourceContent;

  Napi::Value Add(const CallbackInfo& info) {
    Napi::Env env = info.Env();
    HandleScope scope(env);

    if (info.Length() < 1 || !info[0].IsBuffer()) {
      TypeError::New(env, "Expected a string").ThrowAsJavaScriptException();
      return env.Null();
    }

    uint32_t lineOffset = 0;
    if (info.Length() >= 2 && info[1].IsNumber()) {
      lineOffset = info[1].As<Number>().Uint32Value();
    }

    uint32_t columnOffset = 0;
    if (info.Length() >= 3 && info[2].IsNumber()) {
      columnOffset = info[2].As<Number>().Uint32Value();
    }

    auto str = info[0].As<Buffer<char>>();
    // padded_string p(str.c_str(), );
    // ParsedJson pj = simdjson::build_parsed_json(p);
    json_parse(str.Data(), str.Length(), pj);

    if (!pj.is_valid()) {
      Error::New(env, "Invalid JSON").ThrowAsJavaScriptException();
      return env.Null();
    }

    ParsedJson::Iterator pjh(pj);
    if (!pjh.move_to_key("mappings") || !pjh.is_array()) {
      Error::New(env, "Invalid source map").ThrowAsJavaScriptException();
      return env.Null();
    }

    pjh.move_to_index(0);
    do {
      Mapping m;
      m.hasOriginal = false;
      m.hasSource = false;
      m.hasName = false;
      if (!pjh.is_object()) {
        continue; // TODO: throw error??
      }

      if (pjh.move_to_key("source")) {
        if (pjh.is_string()) {
          m.hasSource = true;
          m.source = pjh.get_string();
          if (sources.find(m.source) != sources.end()) {
            sources.emplace(m.source, sources.size());
          }
        }

        pjh.up();
      }

      if (pjh.move_to_key("name")) {
        if (pjh.is_string()) {
          m.hasName = true;
          m.name = pjh.get_string();
        }

        pjh.up();
      }

      if (pjh.move_to_key("original")) {
        if (pjh.is_object()) {
          m.hasOriginal = true;

          if (pjh.move_to_key("line")) {
            if (pjh.is_integer()) {
              m.original.line = pjh.get_integer();
            }

            pjh.up();
          }

          if (pjh.move_to_key("column")) {
            if (pjh.is_integer()) {
              m.original.column = pjh.get_integer();
            }

            pjh.up();
          }
        }

        pjh.up();
      }

      if (pjh.move_to_key("generated")) {
        if (pjh.is_object()) {
          if (pjh.move_to_key("line")) {
            if (pjh.is_integer()) {
              m.generated.line = pjh.get_integer() + lineOffset;
            }

            pjh.up();
          }

          if (pjh.move_to_key("column")) {
            if (pjh.is_integer()) {
              m.generated.column = pjh.get_integer() + columnOffset;
            }

            pjh.up();
          }
        }

        pjh.up();
      }

      mappings.push_back(m);
    } while (pjh.next());
    
    return env.Null();
  }

  Napi::Value Stringify(const CallbackInfo& info) {
    Napi::Env env = info.Env();
    HandleScope scope(env);

    std::stringstream out;
    out << "{\"version\":3,\"sources\":[";

    for (auto it = sources.begin(); it != sources.end(); it++) {
      if (it != sources.begin()) {
        out << ",";
      }
      out << "\"" << it-> first << "\"";
    }

    out << "],\"names\":[],\"mappings\":\"";

    unsigned int previousGeneratedLine = 1;
    unsigned int previousGeneratedColumn = 0;
    unsigned int previousSource = 0;
    unsigned int previousOriginalLine = 0;
    unsigned int previousOriginalColumn = 0;
    Mapping *lastMapping;
    int i = 0;
    for (auto it = mappings.begin(); it != mappings.end(); it++) {
      Mapping &mapping = *it;
      // printf("%d\n", i++);

      if (mapping.generated.line != previousGeneratedLine) {
        previousGeneratedColumn = 0;
        // printf("here\n");
        while (previousGeneratedLine < mapping.generated.line) {
          out << ";";
          previousGeneratedLine++;
        }
        // printf("done\n");
      } else if (it != mappings.begin()) {
        if (!compareByGeneratedPositionsInflated(mapping, *lastMapping)) {
          lastMapping = &(*it);
          continue;
        }
        out << ",";
      }

      base64VLQ_encode(mapping.generated.column - previousGeneratedColumn, out);
      // printf("encoded\n");
      previousGeneratedColumn = mapping.generated.column;

      if (mapping.hasSource) {
        // printf("mapping %s\n", mapping.source.c_str());
        int sourceIdx = sources[mapping.source];
        // printf("encodinggg\n");
        base64VLQ_encode(sourceIdx - previousSource, out);
        // printf("encoded source\n");
        previousSource = sourceIdx;

        base64VLQ_encode(mapping.original.line - 1 - previousOriginalLine, out);
        previousOriginalLine = mapping.original.line - 1;

        base64VLQ_encode(mapping.original.column - previousOriginalColumn, out);
        previousOriginalColumn = mapping.original.column;

        if (mapping.hasName) {
          // int nameIdx = 
        }
      }

      lastMapping = &(*it);
    }

    out << "\"}";

    // printf("%s\n", out.str().c_str());
    return String::New(env, out.str());
  }
};

Napi::FunctionReference SourceMap::constructor;

Object SourceMap::Init(Napi::Env env, Object exports) {
  HandleScope scope(env);

  Function func = DefineClass(env, "SourceMap", {
    InstanceMethod("add", &SourceMap::Add),
    InstanceMethod("stringify", &SourceMap::Stringify)
  });

  constructor = Persistent(func);
  constructor.SuppressDestruct();

  exports.Set("SourceMap", func);
  return exports;
}

Object Init(Env env, Object exports) {
  return SourceMap::Init(env, exports);
}

NODE_API_MODULE(sourcemap, Init)
