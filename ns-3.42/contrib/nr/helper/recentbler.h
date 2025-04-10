// recentbler.h
#ifndef RECENTBLER_H
#define RECENTBLER_H

#pragma once
#include <map>
#include <cstdint>
#include <deque>


// struct RecentBler {
//     uint16_t cellId;
//     uint16_t rnti;
//     double bler;
// };

struct RecentBler
{
    std::deque<bool> recentCorrupted;
    
    void AddSample(bool corrupted) {
        recentCorrupted.push_back(corrupted);
        if (recentCorrupted.size() > 50) {
            recentCorrupted.pop_front();
        }
    }
    
    double GetBler() const {
        if (recentCorrupted.empty()) return 0.0;
        size_t bad = std::count(recentCorrupted.begin(), recentCorrupted.end(), true);
        return static_cast<double>(bad) / recentCorrupted.size();
    }
};

// 外部声明全局映射
extern std::map<uint16_t, std::map<uint16_t, RecentBler>> recentBlerMap; // <CellId, <RNTI, RecentBler>>

#endif // RECENTBLER_H