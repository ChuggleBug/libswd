
#ifndef __OPTIONAL_H
#define __OPTIONAL_H

#include <functional>

template <typename T> class Optional {
  private:
    T value;
    bool exists;

    explicit Optional(const T &val) : value(val), exists(true) {}

  public:
    explicit Optional() : exists(false) {}

    static Optional<T> of(const T &value) { return Optional<T>(value); }

    static Optional<T> none() { return Optional<T>(); }

    inline bool hasValue() const { return exists; }

    inline T getValue() const { return value; }

    void andThen(std::function<void(T)> func) {
        if (hasValue()) {
            func(getValue());
        }
    }

    void andThen(std::function<void(T)> if_func, std::function<void(void)> else_func) {
        if (hasValue()) {
            if_func(getValue());
        } else {
            else_func();
        }
    }

    template <typename K> K map(std::function<K(T)> func) { return func(getValue()); }

    template <typename K> K map(std::function<K(T)> func, K else_val) {
        if (hasValue()) {
            return func(getValue());
        }
        return else_val;
    }
};

#endif // __OPTIONAL_H