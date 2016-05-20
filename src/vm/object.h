// Copyright (c) 2016, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#ifndef VM_OBJECT_H_
#define VM_OBJECT_H_

#include <stdio.h>
#include <stdlib.h>

#include "vm/assert.h"
#include "vm/globals.h"
#include "vm/bitfield.h"
#include "vm/utils.h"

namespace psoup {

enum ObjectAlignment {
  // Alignment offsets are used to determine object age.
  kNewObjectAlignmentOffset = kWordSize,
  kOldObjectAlignmentOffset = 0,
  // Object sizes are aligned to kObjectAlignment.
  kObjectAlignment = 2 * kWordSize,
  kObjectAlignmentLog2 = kWordSizeLog2 + 1,
  kObjectAlignmentMask = kObjectAlignment - 1,
};

enum {
  kSmiTag = 0,
  kHeapObjectTag = 1,
  kSmiTagSize = 1,
  kSmiTagMask = 1,
  kSmiTagShift = 1,
};

enum HeaderBits {
  // During a scavenge: Has this object been copied to to-space?
  kMarkBit = 0,

  // Added to the remembered set. (as-yet-unused)
  kRememberedBit = 1,

  // For symbols.
  kCanonicalBit = 2,

  // (as-yet-unused)
  kInClassTableBit = 3,

  // Is this object the key of an ephemeron? (as-yet-unused)
  kWatchedBit = 4,

  // Should we trap on stores? (as-yet-unused)
  kShallowImmutabilityBit = 5,

  // All slots immutabilty, and transitively contains likewise objects?
  // -> can pass by reference between actors (as-yet-unused)
  kDeepImmutabilityBit = 6,

#if defined(ARCH_IS_32_BIT)
  kSizeFieldOffset = 8,
  kSizeFieldSize = 8,
  kClassIdFieldOffset = 16,
  kClassIdFieldSize = 16,
#elif defined(ARCH_IS_64_BIT)
  kSizeFieldOffset = 16,
  kSizeFieldSize = 16,
  kClassIdFieldOffset = 32,
  kClassIdFieldSize = 32,
#else
#error Unexpected architecture word size
#endif
};


enum {
  kIllegalCid = 0,
  kForwardingCorpseCid = 1,

  kFirstLegalCid = 2,

  kSmiCid = 2,
  kMintCid = 3,
  kBigintCid = 4,
  kFloat64Cid = 5,
  kByteArrayCid = 6,
  kByteStringCid = 7,
  kWideStringCid = 8,
  kArrayCid = 9,
  kWeakArrayCid = 10,
  kEphemeronCid = 11,
  kActivationCid = 12,
  kClosureCid = 13,

  kFirstRegularObjectCid = 14,
};


class Object;
class Array;
class WeakArray;
class Ephemeron;
class ByteString;
class ByteArray;
class WideString;
class Closure;
class Activation;
class Method;
class ObjectStore;
class Behavior;
class Heap;
class AbstractMixin;

#define HEAP_OBJECT_IMPLEMENTATION(klass)                                      \
 private:                                                                      \
  klass* ptr() const {                                                         \
    ASSERT(IsHeapObject());                                                    \
    return reinterpret_cast<klass*>(                                           \
        reinterpret_cast<uword>(this) - kHeapObjectTag);                       \
  }                                                                            \
  friend class Heap;                                                           \
  friend class Object;                                                         \
 public:                                                                       \
  static klass* Cast(Object* object) {                                         \
    return static_cast<klass*>(object);                                        \
  }                                                                            \

class Object {
 public:
  bool IsObject() const {
    return true;
  }
  bool IsForwardingCorpse() const {
    return IsHeapObject() && (cid() == kForwardingCorpseCid);
  }
  bool IsArray() const {
    return IsHeapObject() && (cid() == kArrayCid);
  }
  bool IsByteArray() const {
    return IsHeapObject() && (cid() == kByteArrayCid);
  }
  bool IsByteString() const {
    return IsHeapObject() && (cid() == kByteStringCid);
  }
  bool IsWideString() const {
    return IsHeapObject() && (cid() == kWideStringCid);
  }
  bool IsActivation() const {
    return IsHeapObject() && (cid() == kActivationCid);
  }
  bool IsMediumInteger() const {
    return IsHeapObject() && (cid() == kMintCid);
  }
  bool IsFloat64() const {
    return IsHeapObject() && (cid() == kFloat64Cid);
  }
  bool IsWeakArray() const {
    return IsHeapObject() && (cid() == kWeakArrayCid);
  }
  bool IsEphemeron() const {
    return IsHeapObject() && (cid() == kEphemeronCid);
  }
  bool IsClosure() const {
    return IsHeapObject() && (cid() == kClosureCid);
  }
  bool IsRegularObject() const {
    return IsHeapObject() && (cid() >= kFirstRegularObjectCid);
  }
  bool IsHeapObject() const {
    return (reinterpret_cast<uword>(this) & kSmiTagMask) == kHeapObjectTag;
  }
  bool IsImmediateObject() const { return IsSmallInteger(); }
  bool IsSmallInteger() const {
    return (reinterpret_cast<uword>(this) & kSmiTagMask) == kSmiTag;
  }
  bool IsOldObject() const {
    ASSERT(IsHeapObject());
    uword addr = reinterpret_cast<uword>(this);
    return (addr & kNewObjectAlignmentOffset) == kOldObjectAlignmentOffset;
  }
  bool IsNewObject() const {
    ASSERT(IsHeapObject());
    uword addr = reinterpret_cast<uword>(this);
    return (addr & kNewObjectAlignmentOffset) == kNewObjectAlignmentOffset;
  }
  // Like !IsHeapObject() || IsOldObject(), but compiles to a single branch.
  bool IsImmediateOrOldObject() const {
    COMPILE_ASSERT(kHeapObjectTag == 1);
    COMPILE_ASSERT(kNewObjectAlignmentOffset == kWordSize);
    static const uword kNewObjectBits =
        (kNewObjectAlignmentOffset | kHeapObjectTag);
    const uword addr = reinterpret_cast<uword>(this);
    return (addr & kNewObjectBits) != kNewObjectBits;
  }

  void AssertCouldBeBehavior() const {
    ASSERT(IsRegularObject());
    // 8 slots for a class, 7 slots for a metaclass, plus 1 header.
    intptr_t heap_slots = heap_size() / sizeof(uword);
    ASSERT((heap_slots == 8) || (heap_slots == 10));
  }

  bool is_marked() const {
    return MarkBit::decode(ptr()->header_);
  }
  void set_is_marked(bool value) {
    ptr()->header_ = MarkBit::update(value, ptr()->header_);
  }
  bool is_canonical() const {
    return CanonicalBit::decode(ptr()->header_);
  }
  void set_is_canonical(bool value) {
    ptr()->header_ = CanonicalBit::update(value, ptr()->header_);
  }
  intptr_t heap_size() const {
    return SizeField::decode(ptr()->header_) << kObjectAlignmentLog2;
  }
  intptr_t cid() const {
    return ClassIdField::decode(ptr()->header_);
  }
  void set_cid(intptr_t value) const {
    ptr()->header_ = ClassIdField::update(value, ptr()->header_);
  }
  intptr_t identity_hash() const {
    return ptr()->identity_hash_;
  }
  void set_identity_hash(intptr_t value) {
    ptr()->identity_hash_ = value;
  }

  uword Addr() const {
    return reinterpret_cast<uword>(this) - kHeapObjectTag;
  }
  static Object* FromAddr(uword raw) {
    return reinterpret_cast<Object*>(raw + kHeapObjectTag);
  }

  static Object* InitializeObject(uword raw, intptr_t cid, intptr_t heap_size) {
    ASSERT(cid != kIllegalCid);
    ASSERT((heap_size & kObjectAlignmentMask) == 0);
    ASSERT(heap_size > 0);
    intptr_t size_tag = heap_size >> kObjectAlignmentLog2;
    if (!SizeField::is_valid(size_tag)) {
      size_tag = 0;
      ASSERT(cid < kFirstRegularObjectCid);
    }
    uword header = 0;
    header = SizeField::update(size_tag, header);
    header = ClassIdField::update(cid, header);
    Object* obj = FromAddr(raw);
    obj->ptr()->header_ = header;
    obj->ptr()->identity_hash_ = 0;
    ASSERT(obj->cid() == cid);
    ASSERT(!obj->is_marked());
    return obj;
  }

  template<typename type>
  void StorePointer(type const* addr, type value) {
    *const_cast<type*>(addr) = value;
  }

  intptr_t ClassId() const {
    if (IsSmallInteger()) {
      return kSmiCid;
    } else {
      return cid();
    }
  }

  Behavior* Klass(Heap* heap) const;

  intptr_t HeapSize() {
    ASSERT(IsHeapObject());
    intptr_t heap_size_from_tag = heap_size();
    if (heap_size_from_tag != 0) {
      return heap_size_from_tag;
    }
    return HeapSizeFromClass();
  }
  intptr_t HeapSizeFromClass();
  void Pointers(Object*** from, Object*** to);

  static intptr_t AllocationSize(intptr_t size) {
    return Utils::RoundUp(size, kObjectAlignment);
  }

  const char* ToCString(Heap* heap);
  void Print(Heap* heap);

 private:
  Object* ptr() const {
    ASSERT(IsHeapObject());
    return reinterpret_cast<Object*>(
        reinterpret_cast<uword>(this) - kHeapObjectTag);
  }

  uword header_;
  uword identity_hash_;

  class MarkBit : public BitField<bool, kMarkBit, 1> {};
  class CanonicalBit : public BitField<bool, kCanonicalBit, 1> {};
  class SizeField :
      public BitField<intptr_t, kSizeFieldOffset, kSizeFieldSize> {};
  class ClassIdField :
      public BitField<intptr_t, kClassIdFieldOffset, kClassIdFieldSize> {};
};


class ForwardingCorpse {
 public:
  Object* target() const {
    return ptr()->target_;
  }
  void set_target(Object* value) {
    ptr()->target_ = value;
  }
  intptr_t overflow_size() const {
    return ptr()->overflow_size_;
  }
  void set_overflow_size(intptr_t value) {
    ptr()->overflow_size_ = value;
  }

 protected:
  uword use_header() { return header_; }

 private:
  ForwardingCorpse* ptr() const {
    return reinterpret_cast<ForwardingCorpse*>(
        reinterpret_cast<uword>(this) - kHeapObjectTag);
  }

  uword header_;
  Object* target_;
  intptr_t overflow_size_;
};


class SmallInteger : public Object {
 public:
  static SmallInteger* New(intptr_t value) {
    return reinterpret_cast<SmallInteger*>(value << kSmiTagShift);
  }

  intptr_t value() const {
    ASSERT(IsSmallInteger());
    return reinterpret_cast<intptr_t>(this) >> kSmiTagShift;
  }

  static bool IsSmiValue(int64_t v) {
    return (v >= kSmiMin) && (v <= kSmiMax);
  }

#if defined(ARCH_IS_32_BIT)
  static bool IsSmiValue(intptr_t v) {
    // Check if the top two bits are equal.
    ASSERT(kSmiTagShift == 1);
    return (v ^ (v << 1)) >= 0;
  }
#endif
};


class MediumInteger : public Object {
  HEAP_OBJECT_IMPLEMENTATION(MediumInteger);

 public:
  int64_t value() const {
    return ptr()->value_;
  }
  void set_value(int64_t value) {
    ptr()->value_ = value;
  }

 private:
  int64_t value_;
};


class LargeInteger : public Object {
  HEAP_OBJECT_IMPLEMENTATION(LargeInteger);

 public:
  bool negative() const {
    return ptr()->negative_;
  }
  void set_negative(bool v) {
    ptr()->negative_ = v;
  }

 private:
  intptr_t negative_;
  intptr_t digit_size_;
  uintptr_t digits_[];
};


class RegularObject : public Object {
  HEAP_OBJECT_IMPLEMENTATION(RegularObject);

 public:
  Object* slot(intptr_t i) const {
    return ptr()->slots_[i];
  }
  void set_slot(intptr_t i, Object* value) {
    StorePointer(&ptr()->slots_[i], value);
  }

 private:
  Object** from() const {
    return &ptr()->slots_[0];
  }
  Object* slots_[];
  Object** to() const {
    intptr_t num_slots = (heap_size() - sizeof(Object)) >> kWordSizeLog2;
    return &ptr()->slots_[num_slots - 1];
  }
};


class Array : public Object {
  HEAP_OBJECT_IMPLEMENTATION(Array);

 public:
  SmallInteger* size() const {
    return ptr()->size_;
  }
  void set_size(SmallInteger* s) {
    ptr()->size_ = s;
  }
  Object* element(intptr_t i) const {
    return ptr()->elements_[i];
  }
  void set_element(intptr_t i, Object* value) {
    StorePointer(&ptr()->elements_[i], value);
  }

  intptr_t Size() const {
    return size()->value();
  }

 private:
  Object** from() const {
    return &(ptr()->elements_[0]);
  }
  SmallInteger* size_;
  Object* elements_[];
  Object** to() const {
    return &(ptr()->elements_[Size() - 1]);
  }
};


class WeakArray : public Object {
  HEAP_OBJECT_IMPLEMENTATION(WeakArray);

 public:
  SmallInteger* size() const {
    return ptr()->size_;
  }
  void set_size(SmallInteger* s) {
    ptr()->size_ = s;
  }
  Object* element(intptr_t i) const {
    return ptr()->elements_[i];
  }
  void set_element(intptr_t i, Object* value) {
    StorePointer(&ptr()->elements_[i], value);
  }

  intptr_t Size() const {
    return size()->value();
  }

  // For weak list.
  WeakArray* next() const {
    return reinterpret_cast<WeakArray*>(size());
  }
  void set_next(WeakArray* value) {
    set_size(reinterpret_cast<SmallInteger*>(value));
  }

 private:
  Object** from() const {
    return &(ptr()->elements_[0]);  }
  SmallInteger* size_;
  Object* elements_[];
  Object** to() const {
    return &(ptr()->elements_[Size() - 1]);
  }
};


class Ephemeron : public Object {
  HEAP_OBJECT_IMPLEMENTATION(Ephemeron);

 public:
  Object* key() const { return ptr()->key_; }
  Object* value() const { return ptr()->value_; }
  Object* finalizer() const { return ptr()->finalizer_; }
  Object** key_ptr() const { return &ptr()->key_; }
  Object** value_ptr() const { return &ptr()->value_; }
  Object** finalizer_ptr() const { return &ptr()->finalizer_; }
  void set_key(Object* key) { ptr()->key_ = key; }
  void set_value(Object* value) { ptr()->value_ = value; }
  void set_finalizer(Object* finalizer) { ptr()->finalizer_ = finalizer; }

  // For ephemeron list.
  Ephemeron* next() const {
    return static_cast<Ephemeron*>(key());
  }
  void set_next(Ephemeron* value) {
    set_key(value);
  }

 private:
  Object** from() const {
    return &(ptr()->key_);
  }
  Object* key_;
  Object* value_;
  Object* finalizer_;
    Object** to() const {
    return &(ptr()->finalizer_);
  }
};


class ByteString : public Object {
  HEAP_OBJECT_IMPLEMENTATION(ByteString);

 public:
  static intptr_t hash_random_;

  intptr_t Size() const {
    return size()->value();
  }
  SmallInteger* size() const {
    return ptr()->size_;
  }
  void set_size(SmallInteger* size) {
    ptr()->size_ = size;
  }
  SmallInteger* hash() const {
    return ptr()->hash_;
  }
  void set_hash(SmallInteger* hash) {
    ptr()->hash_ = hash;
  }
  SmallInteger* EnsureHash() {
    if (hash() == 0) {
      // FNV-1a hash
      intptr_t length = Size();
      intptr_t h = length + 1;
      for (intptr_t i = 0; i < length; i++) {
        h = h ^ element(i);
        h = h * 16777619;
      }
      // Random component.
      h = h ^ hash_random_;
      // Smi range on 32-bit.  TODO(rmacnak): kMaxPositiveSmi
      h = h & 0x3FFFFFF;
      ASSERT(h != 0);
      set_hash(SmallInteger::New(h));
    }
    return hash();
  }

  uint8_t element(intptr_t index) const {
    ASSERT(index >= 0 && index < Size());
    return ptr()->elements_[index];
  }
  void set_element(intptr_t index, uint8_t value) {
    ASSERT(index >= 0 && index < Size());
    ptr()->elements_[index] = value;
  }
  uint8_t* element_addr(intptr_t index) {
    return &(ptr()->elements_[index]);
  }

 private:
  SmallInteger* size_;
  SmallInteger* hash_;
  uint8_t elements_[];
};


class WideString : public Object {
  HEAP_OBJECT_IMPLEMENTATION(WideString);

 public:
  intptr_t Size() const {
    return size()->value();
  }
  SmallInteger* size() const {
    return ptr()->size_;
  }
  void set_size(SmallInteger* size) {
    ptr()->size_ = size;
  }
  SmallInteger* hash() const {
    return ptr()->hash_;
  }
  void set_hash(SmallInteger* hash) {
    ptr()->hash_ = hash;
  }
  SmallInteger* EnsureHash() {
    if (hash() == 0) {
      // FNV-1a hash
      intptr_t length = Size();
      intptr_t h = length + 1;
      for (intptr_t i = 0; i < length; i++) {
        h = h ^ element(i);
        h = h * 16777619;
      }
      // Random component.
      h = h ^ ByteString::hash_random_;
      // Smi range on 32-bit.  TODO(rmacnak): kMaxPositiveSmi
      h = h & 0x3FFFFFF;
      ASSERT(h != 0);
      set_hash(SmallInteger::New(h));
    }
    return hash();
  }

  uint32_t element(intptr_t index) const {
    ASSERT(index >= 0 && index < Size());
    return ptr()->elements_[index];
  }
  void set_element(intptr_t index, uint32_t value) {
    ASSERT(index >= 0 && index < Size());
    ptr()->elements_[index] = value;
  }
  uint32_t* element_addr(intptr_t index) {
    return &(ptr()->elements_[index]);
  }

 private:
  SmallInteger* size_;
  SmallInteger* hash_;
  uint32_t elements_[];
};


class ByteArray : public Object {
  HEAP_OBJECT_IMPLEMENTATION(ByteArray);

 public:
  SmallInteger* size() const {
    return ptr()->size_;
  }
  void set_size(SmallInteger* size) {
    ptr()->size_ = size;
  }
  intptr_t Size() const {
    return size()->value();
  }
  uint8_t element(intptr_t index) const {
    return ptr()->elements_[index];
  }
  void set_element(intptr_t index, uint8_t value) {
    ptr()->elements_[index] = value;
  }
  uint8_t* element_addr(intptr_t index) {
    return &(ptr()->elements_[index]);
  }

 private:
  SmallInteger* size_;
  uint8_t elements_[];
};


class Activation : public Object {
  HEAP_OBJECT_IMPLEMENTATION(Activation);

 public:
  static const intptr_t kMaxTemps = 35;

  Activation* sender() const {
    return ptr()->sender_;
  }
  void set_sender(Activation* s) {
    ptr()->sender_ = s;
  }
  SmallInteger* bci() const {
    return ptr()->bci_;
  }
  void set_bci(SmallInteger* i) {
    ptr()->bci_ = i;
  }
  Method* method() const {
    return ptr()->method_;
  }
  void set_method(Method* m) {
    StorePointer(&ptr()->method_, m);
  }
  Closure* closure() const {
    return ptr()->closure_;
  }
  void set_closure(Closure* m) {
    StorePointer(&ptr()->closure_, m);
  }
  Object* receiver() const {
    return ptr()->receiver_;
  }
  void set_receiver(Object* o) {
    StorePointer(&ptr()->receiver_, o);
  }
  intptr_t stack_depth() const {
    return ptr()->stack_depth_->value();
  }
  void set_stack_depth(SmallInteger* d) {
    ptr()->stack_depth_ = d;
  }
  Object* temp(intptr_t index) const {
    return ptr()->temps_[index];
  }
  void set_temp(intptr_t index, Object* o) {
    StorePointer(&ptr()->temps_[index], o);
  }


  Object* Pop() {
    ASSERT(stack_depth() > 0);
    Object* top = ptr()->temps_[stack_depth() - 1];
    set_stack_depth(SmallInteger::New(stack_depth() - 1));
    return top;
  }
  Object* Stack(intptr_t depth) const {
    ASSERT(depth >= 0);
    ASSERT(depth < stack_depth());
    return ptr()->temps_[stack_depth() - depth - 1];
  }
  Object* StackPut(intptr_t depth, Object* o) {
    ASSERT(depth >= 0);
    ASSERT(depth < stack_depth());
    return ptr()->temps_[stack_depth() - depth - 1] = o;
  }
  void PopNAndPush(intptr_t drop_count, Object* value) {
    ASSERT(drop_count >= 0);
    ASSERT(drop_count <= stack_depth());
    set_stack_depth(SmallInteger::New(stack_depth() - drop_count + 1));
    ptr()->temps_[stack_depth() - 1] = value;
  }
  void Push(Object* value) {
    PopNAndPush(0, value);
  }
  void Drop(intptr_t drop_count) {
    ASSERT(drop_count >= 0);
    ASSERT(drop_count <= stack_depth());
    set_stack_depth(SmallInteger::New(stack_depth() - drop_count));
  }
  void Grow(intptr_t grow_count) {
    ASSERT(grow_count >= 0);
    ASSERT(stack_depth() + grow_count < kMaxTemps);
    set_stack_depth(SmallInteger::New(stack_depth() + grow_count));
  }

 private:
  Object** from() const {
    return reinterpret_cast<Object**>(&(ptr()->sender_));
  }
  Activation* sender_;
  SmallInteger* bci_;
  Method* method_;
  Closure* closure_;
  Object* receiver_;
  SmallInteger* stack_depth_;
  Object* temps_[kMaxTemps];
  Object** to() const {
    return reinterpret_cast<Object**>(&(ptr()->temps_[stack_depth() - 1]));
  }
};


class Float64 : public Object {
  HEAP_OBJECT_IMPLEMENTATION(Float64);

 public:
  double value() const {
    return ptr()->value_;
  }
  void set_value(double v) {
    ptr()->value_ = v;
  }

 private:
  double value_;
};


class Closure : public Object {
  HEAP_OBJECT_IMPLEMENTATION(Closure);

 public:
  intptr_t num_copied() const {
    return ptr()->num_copied_->value();
  }
  void set_num_copied(intptr_t v) {
    ptr()->num_copied_ = SmallInteger::New(v);
  }
  Activation* defining_activation() const {
    return ptr()->defining_activation_;
  }
  void set_defining_activation(Activation* a) {
    StorePointer(&ptr()->defining_activation_, a);
  }
  SmallInteger* initial_bci() const {
    return ptr()->initial_bci_;
  }
  void set_initial_bci(SmallInteger* bci) {
    ptr()->initial_bci_ = bci;
  }
  SmallInteger* num_args() const {
    return ptr()->num_args_;
  }
  void set_num_args(SmallInteger* num) {
    ptr()->num_args_ = num;
  }
  Object* copied(intptr_t index) const {
    return ptr()->copied_[index];
  }
  void set_copied(intptr_t index, Object* o) {
    StorePointer(&ptr()->copied_[index], o);
  }

 private:
  Object** from() const {
    return reinterpret_cast<Object**>(&(ptr()->num_copied_));
  }
  SmallInteger* num_copied_;
  Activation* defining_activation_;
  SmallInteger* initial_bci_;
  SmallInteger* num_args_;
  Object* copied_[];
  Object** to() const {
    return reinterpret_cast<Object**>(&(ptr()->copied_[num_copied() - 1]));
  }
};


// === Regular objects with known slot offsets ===


class Behavior : public Object {
  HEAP_OBJECT_IMPLEMENTATION(Behavior);

 public:
  Behavior* superclass() const { return ptr()->superclass_; }
  Array* methods() const { return ptr()->methods_; }
  AbstractMixin* mixin() const { return ptr()->mixin_; }
  Object* enclosing_object() const { return ptr()->enclosing_object_; }
  SmallInteger* id() const { return ptr()->classid_; }
  void set_id(SmallInteger* id) { ptr()->classid_ = id; }
  SmallInteger* format() const { return ptr()->format_; }

 private:
  Behavior* superclass_;
  Array* methods_;
  Object* enclosing_object_;
  AbstractMixin* mixin_;
  SmallInteger* classid_;
  SmallInteger* format_;
};


class Class : public Behavior {
  HEAP_OBJECT_IMPLEMENTATION(Class);

 public:
  ByteString* name() const { return ptr()->name_; }

 private:
  ByteString* name_;
  WeakArray* subclasses_;
};


class Metaclass : public Behavior {
  HEAP_OBJECT_IMPLEMENTATION(Metaclass);

 public:
  Class* this_class() const { return ptr()->this_class_; }

 private:
  Class* this_class_;
};


class AbstractMixin : public Object {
  HEAP_OBJECT_IMPLEMENTATION(AbstractMixin);

 public:
  ByteString* name() const { return ptr()->name_; }
  Array* methods() const { return ptr()->methods_; }
  AbstractMixin* enclosing_mixin() const { return ptr()->enclosing_mixin_; }

 private:
  ByteString* name_;
  Array* methods_;
  AbstractMixin* enclosing_mixin_;
};


class Method : public Object {
  HEAP_OBJECT_IMPLEMENTATION(Method);

 public:
  ByteString* selector() const {
    return ptr()->selector_;
  }
  Array* literals() const {
    return ptr()->literals_;
  }
  ByteArray* bytecode() const {
    return ptr()->bytecode_;
  }
  AbstractMixin* mixin() const {
    return ptr()->mixin_;
  }

  bool IsPublic() const {
    uword header = ptr()->header_->value();
    uword am = header >> 28;
    ASSERT((am == 0) || (am == 1) || (am == 2));
    return am == 0;
  }
  bool IsProtected() const {
    uword header = ptr()->header_->value();
    uword am = header >> 28;
    ASSERT((am == 0) || (am == 1) || (am == 2));
    return am == 1;
  }
  bool IsPrivate() const {
    uword header = ptr()->header_->value();
    uword am = header >> 28;
    ASSERT((am == 0) || (am == 1) || (am == 2));
    return am == 2;
  }
  intptr_t Primitive() const {
    uword header = ptr()->header_->value();
    return (header >> 16) & 1023;
  }
  intptr_t NumArgs() const {
    uword header = ptr()->header_->value();
    return (header >> 0) & 255;
  }
  intptr_t NumTemps() const {
    uword header = ptr()->header_->value();
    return (header >> 8) & 255;
  }

 private:
  SmallInteger* header_;
  Array* literals_;
  ByteArray* bytecode_;
  AbstractMixin* mixin_;
  ByteString* selector_;
  Object* source_;
};


class Message : public Object {
  HEAP_OBJECT_IMPLEMENTATION(Message);

 public:
  void set_selector(ByteString* selector) {
    StorePointer(&ptr()->selector_, selector);
  }
  void set_arguments(Array* arguments) {
    StorePointer(&ptr()->arguments_, arguments);
  }
 private:
  ByteString* selector_;
  Array* arguments_;
};


class Thread : public Object {
  HEAP_OBJECT_IMPLEMENTATION(Thread);

 public:
  Activation* suspended_activation() const {
    return ptr()->suspended_activation_;
  }

 private:
  Activation* suspended_activation_;
};


class Scheduler : public Object {
  HEAP_OBJECT_IMPLEMENTATION(Scheduler);
};


class ObjectStore : public Object {
  HEAP_OBJECT_IMPLEMENTATION(ObjectStore);

 public:
  Object* nil_obj() const { return ptr()->nil_; }
  Object* false_obj() const { return ptr()->false_; }
  Object* true_obj() const { return ptr()->true_; }
  Scheduler* scheduler() const { return ptr()->scheduler_; }
  class ByteString* does_not_understand() const {
    return ptr()->does_not_understand_;
  }
  class ByteString* cannot_return() const {
    return ptr()->cannot_return_;
  }
  class ByteString* about_to_return_through() const {
    return ptr()->about_to_return_through_;
  }
  class ByteString* start() const {
    return ptr()->start_;
  }
  class Array* quick_selectors() const { return ptr()->quick_selectors_; }
  Behavior* Message() const { return ptr()->Message_; }
  Behavior* SmallInteger() const { return ptr()->SmallInteger_; }
  Behavior* MediumInteger() const { return ptr()->MediumInteger_; }
  Behavior* LargeInteger() const { return ptr()->LargeInteger_; }
  Behavior* Float64() const { return ptr()->Float64_; }
  Behavior* ByteArray() const { return ptr()->ByteArray_; }
  Behavior* ByteString() const { return ptr()->ByteString_; }
  Behavior* WideString() const { return ptr()->WideString_; }
  Behavior* Array() const { return ptr()->Array_; }
  Behavior* WeakArray() const { return ptr()->WeakArray_; }
  Behavior* Ephemeron() const { return ptr()->Ephemeron_; }
  Behavior* Activation() const { return ptr()->Activation_; }
  Behavior* Closure() const { return ptr()->Closure_; }

 private:
  class SmallInteger* array_size_;
  Object* nil_;
  Object* false_;
  Object* true_;
  Scheduler* scheduler_;
  class Array* quick_selectors_;
  class ByteString* does_not_understand_;
  class ByteString* must_be_boolean_;
  class ByteString* cannot_return_;
  class ByteString* about_to_return_through_;
  class ByteString* unused_bytecode_;
  class ByteString* start_;
  Behavior* Array_;
  Behavior* ByteArray_;
  Behavior* ByteString_;
  Behavior* WideString_;
  Behavior* Closure_;
  Behavior* Ephemeron_;
  Behavior* Float64_;
  Behavior* LargeInteger_;
  Behavior* MediumInteger_;
  Behavior* Message_;
  Behavior* SmallInteger_;
  Behavior* Thread_;
  Behavior* WeakArray_;
  Behavior* Activation_;
  Behavior* Method_;
  Behavior* Scheduler_;
};

}  // namespace psoup

#endif  // VM_OBJECT_H_
