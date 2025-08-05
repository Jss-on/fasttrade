#include "fasttrade/utils/decimal.hpp"
#include <sstream>
#include <stdexcept>
#include <cmath>
#include <algorithm>

namespace fasttrade::utils {

Decimal::Decimal(const std::string& str) {
    if (str.empty()) {
        value_ = 0;
        return;
    }
    
    // Handle negative numbers
    bool negative = false;
    std::string clean_str = str;
    if (clean_str[0] == '-') {
        negative = true;
        clean_str = clean_str.substr(1);
    } else if (clean_str[0] == '+') {
        clean_str = clean_str.substr(1);
    }
    
    // Find decimal point
    size_t decimal_pos = clean_str.find('.');
    std::string integer_part;
    std::string fractional_part;
    
    if (decimal_pos == std::string::npos) {
        // No decimal point
        integer_part = clean_str;
        fractional_part = "";
    } else {
        integer_part = clean_str.substr(0, decimal_pos);
        fractional_part = clean_str.substr(decimal_pos + 1);
    }
    
    // Parse integer part
    int64_t int_value = 0;
    if (!integer_part.empty()) {
        try {
            int_value = std::stoll(integer_part);
        } catch (const std::exception&) {
            throw std::invalid_argument("Invalid decimal string: " + str);
        }
    }
    
    // Parse fractional part
    int64_t frac_value = 0;
    if (!fractional_part.empty()) {
        // Pad or truncate to 18 digits
        if (fractional_part.length() > 18) {
            fractional_part = fractional_part.substr(0, 18);
        } else {
            fractional_part.append(18 - fractional_part.length(), '0');
        }
        
        try {
            frac_value = std::stoll(fractional_part);
        } catch (const std::exception&) {
            throw std::invalid_argument("Invalid decimal string: " + str);
        }
    }
    
    value_ = int_value * SCALE_FACTOR + frac_value;
    if (negative) {
        value_ = -value_;
    }
}

std::string Decimal::to_string() const {
    if (value_ == 0) {
        return "0";
    }
    
    bool negative = value_ < 0;
    int64_t abs_value = negative ? -value_ : value_;
    
    int64_t integer_part = abs_value / SCALE_FACTOR;
    int64_t fractional_part = abs_value % SCALE_FACTOR;
    
    std::ostringstream oss;
    if (negative) {
        oss << '-';
    }
    oss << integer_part;
    
    if (fractional_part > 0) {
        oss << '.';
        
        // Convert fractional part to string with leading zeros
        std::string frac_str = std::to_string(fractional_part);
        // Pad with leading zeros to make it 18 digits
        frac_str = std::string(18 - frac_str.length(), '0') + frac_str;
        
        // Remove trailing zeros
        while (!frac_str.empty() && frac_str.back() == '0') {
            frac_str.pop_back();
        }
        
        oss << frac_str;
    }
    
    return oss.str();
}

std::ostream& operator<<(std::ostream& os, const Decimal& decimal) {
    os << decimal.to_string();
    return os;
}

std::istream& operator>>(std::istream& is, Decimal& decimal) {
    std::string str;
    is >> str;
    decimal = Decimal::from_string(str);
    return is;
}

} // namespace fasttrade::utils
