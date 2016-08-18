#pragma once

#include "ExtTimingTags.hpp"

//Define to enable extra output related to tag merging
//#define DEBUG_TAG_MERGE

using Tags = ExtTimingTags;

class TagReducer {
    public:
        virtual Tags merge_tags(NodeId node_id, const Tags& orig_tags) const = 0;
        virtual Tags merge_tags(NodeId node_id, const Tags& orig_tags, const double delay_bin_size) const = 0;
        virtual Tags merge_max_tags(const Tags& orig_tags, int num_nodes) const = 0;
        virtual double default_bin_size() const = 0;
        virtual double default_slack_threshold() const = 0;
};

class NoOpTagReducer : public TagReducer {

        Tags merge_tags(NodeId /*unused*/, const Tags& orig_tags) const override { return orig_tags; }
        Tags merge_tags(NodeId /*unused*/, const Tags& orig_tags, const double /*unused*/) const override { return orig_tags; }
        Tags merge_max_tags(const Tags& orig_tags, int num_nodes) const { return orig_tags; }
        double default_bin_size() const { return 0.; }
        double default_slack_threshold() const { return 0.; }
};


template<typename AnalyzerType>
class StaSlackTagReducer : public TagReducer {

    public:
        StaSlackTagReducer(std::shared_ptr<AnalyzerType> analyzer, double slack_threshold, double delay_bin_size)
            : analyzer_(analyzer)
            , slack_threshold_(slack_threshold)
            , delay_bin_size_(delay_bin_size)
            {}

        //Merge tags with the default bin size
        Tags merge_tags(NodeId node_id, const Tags& orig_tags) const override {
            return merge_tags(node_id, orig_tags, delay_bin_size_);
        }


        //Merge tags with a specific bin size
        Tags merge_tags(NodeId node_id, const Tags& orig_tags, const double delay_bin_size) const override {
            //Calculate the slack from standard STA
            auto sta_tag = *(analyzer_->setup_data_tags(node_id).begin());
            auto req = sta_tag.req_time().value();

            auto arr_threshold = req - slack_threshold_;

            return merge_tags(orig_tags, delay_bin_size, arr_threshold);
        }

        Tags merge_max_tags(const Tags& orig_tags, int num_nodes) const override {
            auto max_req = 0.;

            for(NodeId node_id = 0; node_id < num_nodes; ++node_id) {
                auto sta_tag = *(analyzer_->setup_data_tags(node_id).begin());
                auto req = sta_tag.req_time().value();

                max_req = std::max(max_req, req);
            }

            auto arr_threshold = max_req - slack_threshold_;

            return merge_tags(orig_tags, delay_bin_size_, arr_threshold);
        }

        double default_bin_size() const override { return delay_bin_size_; }
        double default_slack_threshold() const override { return slack_threshold_; }

        Tags merge_tags(const Tags& orig_tags, const double delay_bin_size, const double arr_threshold) const {
            Tags merged_tags;

            for(const auto& tag : orig_tags) {

                if(tag->arr_time().value() > arr_threshold) {
                    //Above threshold do not merge
                    merged_tags.add_tag(tag);
#ifdef DEBUG_TAG_MERGE
                    std::cout << "Above ARR threshold (" << tag->arr_time().value() << " > " << arr_threshold << "): adding tag " << tag->trans_type() << "@" << tag->arr_time().value() << "\n";
#endif
                } else {
                    //Below threshold, merge if possible
                    auto bin_tag_pred = [&](std::shared_ptr<const typename Tags::Tag> search_tag) {
                        if(tag->trans_type() != search_tag->trans_type()) return false; //Require the same transition
                        if(search_tag->arr_time().value() > arr_threshold) return false; //Don't merge with tags beyond threshold

                        //Map to the appropriate bin, we treat a bin size of zero as no binning
                        double tag_bin = tag->arr_time().value();
                        double search_tag_bin = search_tag->arr_time().value();

                        if(delay_bin_size != 0.) {
                            tag_bin = std::floor(tag_bin / delay_bin_size);
                            search_tag_bin = std::floor(search_tag_bin / delay_bin_size);
                        }

                        if(tag_bin != search_tag_bin) return false;
                        return true;
                    };

                    //Get any tag in the same bin (if such a tag exists)
                    auto iter = std::find_if(merged_tags.begin(), merged_tags.end(), bin_tag_pred);

                    if(iter != merged_tags.end()) { //Found existing
#ifdef DEBUG_TAG_MERGE
                        auto matched_tag = *iter;
                        std::cout << "Below ARR threshold (" << arr_threshold << ") match: merging new tag " << tag->trans_type() << "@" << tag->arr_time().value();
                        std::cout << " with existing " << matched_tag->trans_type() << "@" << matched_tag->arr_time().value() << "\n";
#endif
                        merged_tags.max_arr(iter, tag);

                    } else { //Start a new bin
#ifdef DEBUG_TAG_MERGE
                        std::cout << "Below ARR threshold no match: adding tag " << tag->trans_type() << "@" << tag->arr_time().value() << "\n";
#endif
                        merged_tags.add_tag(tag);
                    }
                }
            }
            
            return merged_tags;
        }
    private:
        std::shared_ptr<AnalyzerType> analyzer_;
        double slack_threshold_;
        double delay_bin_size_;
};

