#include "GameMap.h"
#include <fstream>
#include <iostream>

// This function signature now matches the one in GameMap.h
// Requirements: QCSIDM_SRS_003, QCSIDM_SRS_013, QCSIDM_SRS_014, QCSIDM_SRS_018, QCSIDM_SRS_020, QCSIDM_SRS_021, QCSIDM_SRS_025, QCSIDM_SRS_030, QCSIDM_SRS_032, QCSIDM_SRS_033, QCSIDM_SRS_034, QCSIDM_SRS_038, QCSIDM_SRS_040, QCSIDM_SRS_057, QCSIDM_SRS_063, QCSIDM_SRS_074, QCSIDM_SRS_075, QCSIDM_SRS_076, QCSIDM_SRS_085, QCSIDM_SRS_104, QCSIDM_SRS_111, QCSIDM_SRS_122, QCSIDM_SRS_127, QCSIDM_SRS_131, QCSIDM_SRS_136, QCSIDM_SRS_140, QCSIDM_SRS_155, QCSIDM_SRS_162, QCSIDM_SRS_190
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
    
// Requirements: QCSIDM_SRS_003, QCSIDM_SRS_013, QCSIDM_SRS_014, QCSIDM_SRS_018, QCSIDM_SRS_020, QCSIDM_SRS_021, QCSIDM_SRS_025, QCSIDM_SRS_030, QCSIDM_SRS_032, QCSIDM_SRS_033, QCSIDM_SRS_034, QCSIDM_SRS_038, QCSIDM_SRS_040, QCSIDM_SRS_057, QCSIDM_SRS_063, QCSIDM_SRS_074, QCSIDM_SRS_075, QCSIDM_SRS_076, QCSIDM_SRS_085, QCSIDM_SRS_104, QCSIDM_SRS_111, QCSIDM_SRS_122, QCSIDM_SRS_127, QCSIDM_SRS_131, QCSIDM_SRS_136, QCSIDM_SRS_140, QCSIDM_SRS_155, QCSIDM_SRS_162, QCSIDM_SRS_190
    std::cout << "Server Map loaded: " << width_ << "x" << height_ << std::endl;
    return true;
}

// Requirements: QCSIDM_SRS_003, QCSIDM_SRS_013, QCSIDM_SRS_014, QCSIDM_SRS_018, QCSIDM_SRS_020, QCSIDM_SRS_021, QCSIDM_SRS_025, QCSIDM_SRS_030, QCSIDM_SRS_032, QCSIDM_SRS_033, QCSIDM_SRS_034, QCSIDM_SRS_040, QCSIDM_SRS_057, QCSIDM_SRS_063, QCSIDM_SRS_074, QCSIDM_SRS_075, QCSIDM_SRS_076, QCSIDM_SRS_085, QCSIDM_SRS_111, QCSIDM_SRS_122, QCSIDM_SRS_131, QCSIDM_SRS_137, QCSIDM_SRS_155, QCSIDM_SRS_164, QCSIDM_SRS_180, QCSIDM_SRS_190
bool GameMap::is_walkable(int x, int y) const {
    if (x < 0 || x >= width_ || y < 0 || y >= height_) {
        return false;
    }
    return tiles_[y][x] != 1;
}