#pragma once
#include <vector>
#include <string>

class GameMap {
public:
    // This declaration was missing
    bool load(const std::string& filename, std::vector<std::vector<int>>& layout);
    bool is_walkable(int x, int y) const;

private:
    std::vector<std::vector<int>> tiles_;
    int width_ = 0;
    int height_ = 0;
};