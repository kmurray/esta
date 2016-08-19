#include "TagPermutationGenerator.hpp"

TagPermutationGenerator::TagPermutationGenerator(std::vector<ExtTimingTags> input_tag_sets)
    : input_tag_sets_(input_tag_sets)
    , iterators_(input_tag_sets_.size())
    , done_(false) {
    initialize();    
}

void TagPermutationGenerator::initialize() {
    for(size_t i = 0; i < input_tag_sets_.size(); i++) {
        iterators_[i] = input_tag_sets_[i].begin();
    }
}

void TagPermutationGenerator::advance() {
    bool carry = true; //Initialize a carry in at input 0
    for(size_t i = 0; i < input_tag_sets_.size(); i++) {
        if(carry) {
            //Advance by one step
            ++iterators_[i];
            carry = false;
        }

        if(iterators_[i] == input_tag_sets_[i].end()) {
            //If we are at the end, carry to the next index
            carry=true;

            //And reset the current
            iterators_[i] = input_tag_sets_[i].begin();
        }
    }
    if(carry == true) {
        //Overflow, indicating that we have finished iteration
        done_ = true;
    }
}

std::vector<ExtTimingTag::cptr> TagPermutationGenerator::next() {
    std::vector<ExtTimingTag::cptr> input_tag_perm(input_tag_sets_.size(), nullptr);

    for(size_t i = 0; i < input_tag_sets_.size(); i++) {
        input_tag_perm[i] = (*iterators_[i]);
    }

    advance();

    return input_tag_perm;
}

bool TagPermutationGenerator::done() {
    return done_;
}

size_t TagPermutationGenerator::num_permutations() {
    size_t count = 1;
    for(size_t i = 0 ; i < input_tag_sets_.size(); i++) {
        count *= input_tag_sets_[i].num_tags();    
    }
    return count;
}
