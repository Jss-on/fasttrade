#pragma once

#include <string>
#include <cstdint>
#include <iostream>

namespace fasttrade::utils {

/**
 * @brief High-precision decimal number for financial calculations
 * 
 * This class provides accurate decimal arithmetic for trading operations,
 * avoiding floating-point precision issues that are critical in finance.
 */
class Decimal {
private:
    static constexpr int64_t SCALE_FACTOR = 1000000000000000000LL; // 18 decimal places
    int64_t value_;

public:
    // Constructors
    Decimal() : value_(0) {}
    explicit Decimal(int64_t value) : value_(value * SCALE_FACTOR) {}
    explicit Decimal(double value) : value_(static_cast<int64_t>(value * SCALE_FACTOR)) {}
    explicit Decimal(const std::string& str);
    
    // Copy/Move constructors
    Decimal(const Decimal& other) = default;
    Decimal(Decimal&& other) noexcept = default;
    Decimal& operator=(const Decimal& other) = default;
    Decimal& operator=(Decimal&& other) noexcept = default;

    // Arithmetic operators
    Decimal operator+(const Decimal& other) const {
        Decimal result;
        result.value_ = value_ + other.value_;
        return result;
    }

    Decimal operator-(const Decimal& other) const {
        Decimal result;
        result.value_ = value_ - other.value_;
        return result;
    }

    Decimal operator*(const Decimal& other) const {
        Decimal result;
        result.value_ = (value_ * other.value_) / SCALE_FACTOR;
        return result;
    }

    Decimal operator/(const Decimal& other) const {
        Decimal result;
        result.value_ = (value_ * SCALE_FACTOR) / other.value_;
        return result;
    }

    // Compound assignment operators
    Decimal& operator+=(const Decimal& other) {
        value_ += other.value_;
        return *this;
    }

    Decimal& operator-=(const Decimal& other) {
        value_ -= other.value_;
        return *this;
    }

    Decimal operator-() const {
        return Decimal(-value_);
    }

    // Comparison operators
    bool operator==(const Decimal& other) const { return value_ == other.value_; }
    bool operator!=(const Decimal& other) const { return value_ != other.value_; }
    bool operator<(const Decimal& other) const { return value_ < other.value_; }
    bool operator<=(const Decimal& other) const { return value_ <= other.value_; }
    bool operator>(const Decimal& other) const { return value_ > other.value_; }
    bool operator>=(const Decimal& other) const { return value_ >= other.value_; }

    // Conversion methods
    double to_double() const { return static_cast<double>(value_) / SCALE_FACTOR; }
    std::string to_string() const;
    int64_t to_int64() const { return value_ / SCALE_FACTOR; }

    // Utility methods
    Decimal abs() const {
        Decimal result;
        result.value_ = (value_ < 0) ? -value_ : value_;
        return result;
    }

    bool is_zero() const { return value_ == 0; }
    bool is_positive() const { return value_ > 0; }
    bool is_negative() const { return value_ < 0; }

    // Factory methods
    static Decimal zero() { return Decimal(); }
    static Decimal from_string(const std::string& str) { return Decimal(str); }
};

// Stream operators
std::ostream& operator<<(std::ostream& os, const Decimal& decimal);
std::istream& operator>>(std::istream& is, Decimal& decimal);

} // namespace fasttrade::utils
