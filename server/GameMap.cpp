#include "GameMap.h"
#include <fstream>
#include <iostream>

// This function signature now matches the one in GameMap.h
bool GameMap::load(const std::string& filename, std::vector<std::vector<int>>& layout) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open map file: " << filename << std::endl;
        return false;
    }

    layout.clear();
    tiles_.clear();
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        
        std::vector<int> row;
        for (char c : line) {
            if (c >= '0' && c <= '9') {
                row.push_back(c - '0');
            }
        }
        layout.push_back(row);
        tiles_.push_back(row);
    }

    if (!tiles_.empty()) {
        height_ = tiles_.size();
        width_ = tiles_[0].size();
    }
    
    std::cout << "Server Map loaded: " << width_ << "x" << height_ << std::endl;
    return true;
}

bool GameMap::is_walkable(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        return false;
    }
    return tiles_[y][x] != 1;
}