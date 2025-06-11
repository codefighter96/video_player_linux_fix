#pragma once

#include <cstddef>
#include <memory>
#include <stdexcept>
#include <type_traits>

//==============================================================================
// 1) pointer_access<P> defines the two abstract operations:
//       - get(p):    returns a T* (or nullptr) no matter what P is
//       - reset(p,r):   (re)assigns p from raw pointer r
//==============================================================================

template<typename P, typename Enable = void> struct pointer_access;

// — raw pointers —
template<typename T> struct pointer_access<T*> {
    using element_type = T;
    static T* get(T* p) { return p; }
    static void reset(T*& p, T* raw) { p = raw; }
};

// — std::weak_ptr<T> —
template<typename T> struct pointer_access<std::weak_ptr<T>> {
    using element_type = T;
    // lock() -> shared_ptr -> get()
    static T* get(const std::weak_ptr<T>& p) {
      if (auto sp = p.lock()) return sp.get();
      return nullptr;
    }
    // reset() simply clears the weak_ptr
    static void reset(std::weak_ptr<T>& p, T*) { p.reset(); }
};

// — any P that has get() and reset(T*) members —
template<typename P>
struct pointer_access<
  P,
  std::void_t<
    decltype(std::declval<P>().get()),
    decltype(std::declval<P>().reset(std::declval<typename P::element_type*>()))>> {
    using element_type = typename P::element_type;
    static element_type* get(const P& p) { return p.get(); }
    static void reset(P& p, element_type* raw) { p.reset(raw); }
};

/// "Smarter" pointers
/// When calling a method on a smarter pointer, it will check whether the
/// underlying pointer is null and if so, it will throw an exception.
///
/// I made this because the "real" smart pointers are clearly not smart enough.
template<typename T, typename P>
// Template typename P to indicate the type of pointer,
// so later it can be used with shared_ptr, unique_ptr and weak_ptr
// Template typename T to indicate the type of object that the pointer points to
class smarter_ptr {
    static_assert(!std::is_reference_v<P>, "P must not be a reference");
    // static_assert(std::is_destructible<T>(), "T must be destructible");

  private:
    P _ptr;

    inline void check() const {
      if (!pointer_access<P>::get(_ptr)) throw std::runtime_error("Dereferencing a null pointer");
    }

  public:
    // default / null
    smarter_ptr() = default;
    smarter_ptr(std::nullptr_t)
      : _ptr{} {}

    // from raw pointer
    explicit smarter_ptr(T* raw)
      : _ptr{} {
      if (raw) pointer_access<P>::reset(_ptr, raw);
    }

    // from any P
    smarter_ptr(const P& other)
      : _ptr(other) {}

    // deref / arrow
    T& operator*() const {
      check();
      return *pointer_access<P>::get(_ptr);
    }
    T* operator->() const {
      check();
      return pointer_access<P>::get(_ptr);
    }

    // assignment
    // smarter_ptr& operator=(const P& other) {
    //     _ptr = other;
    //     return *this;
    // }
    smarter_ptr& operator=(T& raw) {
      pointer_access<P>::reset(_ptr, &raw);
      return *this;
    }
    smarter_ptr& operator=(T* raw) {
      pointer_access<P>::reset(_ptr, raw);
      return *this;
    }
    smarter_ptr& operator=(std::nullptr_t) {
      pointer_access<P>::reset(_ptr, nullptr);
      return *this;
    }

    // implicit conversion back to P or raw T*
    // operator P() const {
    //     return _ptr;
    // }
    operator T*() const { return pointer_access<P>::get(_ptr); }

    // boolean test
    explicit operator bool() const { return pointer_access<P>::get(_ptr) != nullptr; }

    // comparisons
    bool operator==(const smarter_ptr& o) const {
      return pointer_access<P>::get(_ptr) == pointer_access<P>::get(o._ptr);
    }
    bool operator!=(const smarter_ptr& o) const { return !(*this == o); }
    bool operator==(std::nullptr_t) const { return pointer_access<P>::get(_ptr) == nullptr; }
    bool operator!=(std::nullptr_t) const { return pointer_access<P>::get(_ptr) != nullptr; }
};

//==============================================================================
// convenience aliases
//==============================================================================

// TODO: test those smarter_smart pointers
template<typename T> using smarter_shared_ptr = smarter_ptr<T, std::shared_ptr<T>>;
template<typename T> using smarter_unique_ptr = smarter_ptr<T, std::unique_ptr<T>>;
template<typename T> using smarter_weak_ptr = smarter_ptr<T, std::weak_ptr<T>>;
template<typename T> using smarter_raw_ptr = smarter_ptr<T, T*>;
