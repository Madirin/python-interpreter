#pragma once

#include <string>
#include <vector>
#include <iostream>


class ErrorReporter {
public:
    ErrorReporter() = default;

    void add_error(const std::string &message) {
        errors.push_back(message);
    }

    bool has_errors() const {
        return !errors.empty();
    }

    void print_errors(std::ostream &out = std::cerr) const {
        for (const auto &err : errors) {
            out << "Error: " << err << "\n";
        }
    }

    void clear() {
        errors.clear();
    }

private:
    std::vector<std::string> errors;
};
