#pragma once
#include <tclap/Constraint.h>
#include "../namespaces/filesystem.h"

class TestSuitConstraint : public TCLAP::Constraint<std::string> {

public:

    /**
    * Returns a description of the Constraint.
    */
    std::string description() const override {
        return "Input contains " + file::illegal_chars + " in the name.";
    }

    /**
    * Returns the short ID for the Constraint.
    */
    std::string shortID() const override {
        return "suit name";
    }

    /**
    * The method used to verify that the value parsed from the command
    * line meets the constraint.
    * \param value - The value that will be checked.
    */
    bool check(const std::string& value) const override {
        return file::is_name_legal(value) && value != "input";
    }

};
