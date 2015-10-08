#pragma once
#include <string>
#include <unordered_map>

class StringTable {
    public:
        ~StringTable() {
            for(auto& kv : table_) {
                delete kv.second;
            }
        }

        std::string* make_str(char* str) {
            auto iter = table_.find(str);
            if(iter != table_.end()) {
                //A hit return the existing string
                return iter->second;
            } else {
                //A miss, add the string to the table
                std::string* new_str = new std::string(str);

                //Insert it
                table_[*new_str] = new_str;
                return new_str;
            }
        }
        
    private:
        std::unordered_map<std::string,std::string*> table_;
};
