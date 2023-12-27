#include <algorithm>
#include <cstdio>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include "vm/primordial_soup.h"
#include "vm/virtual_memory.h"
#include "vm/heap.h"
#include "vm/snapshot.h"
#include "vm/object.h"
#include "vm/isolate.h"
#include "vm/interpreter.h"

std::string class_name(psoup::Heap *heap, intptr_t cid) {
  psoup::Behavior cls = heap->ClassAt(cid);
  psoup::Behavior theMetaclass = heap->ClassAt(psoup::kSmiCid)->Klass(heap)->Klass(heap);
  if (cls->Klass(heap) == theMetaclass) {
    // A Metaclass.
    psoup::String name = static_cast<psoup::Metaclass>(cls)->this_class()->name();
    if (!name->IsString()) {
      return "Uninitialized metaclass?";
    } else {
      return std::string(reinterpret_cast<char*>(name->element_addr(0)), name->Size()) + " class";
    }
  } else {
    // A Class.
    psoup::String name = static_cast<psoup::Class>(cls)->name();
    if (!name->IsString()) {
      return "Uninitialized class?";
    } else {
      return std::string(reinterpret_cast<char*>(name->element_addr(0)), name->Size());
    }
  }
}


void analyze(psoup::VirtualMemory &snapshot) {
  using size = std::tuple<int /* count */, int /* bytes */>;
  using entry = std::tuple<intptr_t, int, int>;

  // Load the snapshot
  PrimordialSoup_Startup();
  psoup::Heap *heap = new psoup::Heap();
  heap->InitializeInterpreter(new psoup::Interpreter(heap, nullptr));
  {
    psoup::Deserializer deserializer(heap,
                                     reinterpret_cast<void*>(snapshot.base()),
                                     snapshot.size());
    deserializer.Deserialize();
  }

  // Count instances
  std::unordered_map<intptr_t, size> results;
  heap->Walk([&](psoup::HeapObject h) {
    size prior = results[h->ClassId()];
    std::get<0>(prior)++;
    std::get<1>(prior) += h->HeapSize();
    results[h->ClassId()] = prior;
  });

  // Sort
  std::vector<entry> entries;
  for (auto &it : results) {
    entries.push_back({it.first, std::get<0>(it.second), std::get<1>(it.second)});
  }
  std::sort(entries.begin(), entries.end(), [](entry a, entry b) { return std::get<1>(a) > std::get<1>(b); });

  // Print results
  int total_count = 0, total_bytes = 0;
  printf("%60s %10s %10s\n", "Class", "Instances", "Bytes");
  for (auto &entry : entries) {
    std::string name = class_name(heap, std::get<0>(entry));
    int count = std::get<1>(entry), bytes = std::get<2>(entry);
    printf("%60s %10d %10d\n", name.c_str(), count, bytes);
    total_count += count;
    total_bytes += bytes;
  }
  printf("%60s %10d %10d\n", "Total", total_count, total_bytes);
}
