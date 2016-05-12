#pragma once
#include <vector>
#include "ExtTimingTags.hpp"

//Lazily generates all permutations of tags in the input_tag_sets
class TagPermutationGenerator {
    
    public:
        TagPermutationGenerator(std::vector<ExtTimingTags> input_tag_sets);

        //Is the generator finished
        bool done();

        //Gets the next permutation
        std::vector<const ExtTimingTag*> next();

    private:
        void initialize();
        void advance();


        //The set of input tags
        std::vector<ExtTimingTags> input_tag_sets_;


        //The generator state, we keep an iterator for each 'input'
        //which we advance through piecewise (i.e. like binary increment)
        std::vector<ExtTimingTags::iterator> iterators_;
        bool done_;
};
