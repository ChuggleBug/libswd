
#ifndef __OPTIONAL_H
#define __OPTIONAL_H

template <typename T>
class Optional {
  private:
    T value;
    bool exists;

    explicit Optional() : exists(false) {}
    explicit Optional(const T& val) : value(val), exists(true) {}

  public:

    static Optional<T> of(const T& value) {
        return Optional<T>(value);
    }

    static Optional<T> none() {
        return Optional<T>();
    }

    bool hasValue() const {
        return exists;
    }

    T getValue() const {
        return value;
    }
};

#endif // __OPTIONAL_H