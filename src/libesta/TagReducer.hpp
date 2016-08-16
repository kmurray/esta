#pragma once

#include "ExtTimingTags.hpp"

using Tags = ExtTimingTags;

class TagReducer {
    public:
        virtual Tags merge_tags(NodeId node_id, const Tags& orig_tags) const = 0;
};

class NoOpTagReducer : public TagReducer {

        Tags merge_tags(NodeId /*unused*/, const Tags& orig_tags) const override { return orig_tags; }
};

class FixedBinTagReducer : public TagReducer {
    public:

        FixedBinTagReducer(double delay_bin_size)
            : delay_bin_size_(delay_bin_size)
        {}

        Tags merge_tags(NodeId /*unused*/, const Tags& orig_tags) const override {
            Tags merged_tags;

            for(const auto& tag : orig_tags) {
                //Predicate for finding an existing tag in the same bin
                auto bin_tag_pred = [&](std::shared_ptr<const typename Tags::Tag> search_tag) {
                    if(tag->trans_type() != search_tag->trans_type()) return false;
                    
                    //Initialize to the arrival time
                    double tag_bin = tag->arr_time().value();
                    double search_tag_bin = search_tag->arr_time().value();

                    if(delay_bin_size_ != 0.) {
                        //Map to the appropriate bin, we treat a bin size of zero as no binning
                        tag_bin = std::floor(tag_bin / delay_bin_size_);
                        search_tag_bin = std::floor(search_tag_bin / delay_bin_size_);
                    }

                    if(tag_bin != search_tag_bin) return false;

                    //Matched transition and delay bin
                    return true;
                };

                //Get any tag in the same bin (if such a tag exists)
                auto iter = std::find_if(merged_tags.begin(), merged_tags.end(), bin_tag_pred);

                if(iter != merged_tags.end()) { //Found existing
                    merged_tags.max_arr(iter, tag);

                } else { //Start a new bin
                    merged_tags.add_tag(tag);
                }
            }

            return merged_tags;
        }

    private:
        double delay_bin_size_;
};



#if 0
template<class Tags>
class MaxPermsTagMerger : public TagMerger<Tags> {
    public:

        MaxPermsTagMerger(size_t max_permutations)
            : max_permutations_(max_permutations)
            {}

        Tags merge_tags(NodeId node_id, const Tags& orig_tags) const override {

            Tags reduced_tags;

            double delay_bin_size = ;
            
            while(num_permutations > max_permutations_) {
                FixedBinTagMerger bin_merger(delay_bin_size);
                
                reduced_tags = bin_merger.merge_tags(orig_tags);

                delay_bins_size *= ;
            }

            return reduced_tags;
        }

    private:
        size_t max_permutations_;
};
#endif
