#pragma once
#include <string>
#include <chrono>
#include <map>
#include <stack>
#include <vector>
#include <iostream>
#include <cassert>

class ActionTimer {
    using steady_clock = std::chrono::steady_clock;
    using time_point = std::chrono::time_point<steady_clock>;
    public:
        ActionTimer(std::ostream& os = std::cout): os_(os) {}
        ~ActionTimer() {
            while(!action_stack_.empty()) {
                auto action_name = action_stack_.top();
                action_stack_.pop();
                auto iter = timers_.find(action_name);
                if(iter != timers_.end()) {
                    pop_timer(action_name);
                }
            }
        }

        void push_timer(const std::string& action_name, bool print=true) {
            if(print) {
                os_ << "### Start " << action_name << "..." << std::endl;
            }
            assert(timers_.find(action_name) == timers_.end());

            timers_[action_name] = std::chrono::steady_clock::now();
            action_stack_.push(action_name);
        }

        float elapsed(const std::string& action_name) const {
            auto iter = timers_.find(action_name);
            assert(iter != timers_.end());

            return duration(steady_clock::now(), iter->second);
        }

        float pop_timer(const std::string& action_name, bool print=true) {
            using namespace std::chrono;

            auto iter = timers_.find(action_name);
            assert(iter != timers_.end());

            float val = duration(steady_clock::now(), iter->second);

            if(print) {
                os_ << "### End   " << action_name << " after " << val << " sec" << std::endl;
            }

            timers_.erase(iter);

            return val;
        }
    private:
        float duration(const time_point end, const time_point start) const {
            return std::chrono::duration_cast<std::chrono::duration<float>>(end - start).count();
        }

    private:
        std::map<const std::string,time_point> timers_;
        std::stack<std::string> action_stack_;
        std::ostream& os_;
};

struct EtaStats {
    float approx_time = 0;
    float approx_eval_time = 0;
    unsigned long long approx_attempts = 0;
    unsigned long long approx_accepted = 0;
};
