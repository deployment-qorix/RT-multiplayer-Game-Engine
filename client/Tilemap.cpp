#include "Tilemap.h"
#include <fstream>
#include <iostream>
bool Tilemap::load(const std::string& tilesetPath, sf::Vector2u tileSize, const std::string& mapFile, std::vector<std::vector<int>>& layout) {
    if (!m_tileset.loadFromFile(tilesetPath)) return false;
    std::ifstream file(mapFile);
    if (!file.is_open()) return false;
    layout.clear();
    std::string line;
    while (std::getline(file, line)) {
        std::vector<int> row;
        for (char c : line) {
            if (c >= '0' && c <= '9') row.push_back(c - '0');
        }
        layout.push_back(row);
    }
    file.close();
    unsigned int width = layout.empty() ? 0 : layout[0].size();
    unsigned int height = layout.size();
    m_vertices.setPrimitiveType(sf::Quads);
    m_vertices.resize(width * height * 4);
    for (unsigned int j = 0; j < height; ++j) {
        for (unsigned int i = 0; i < width; ++i) {
            int tileNumber = layout[j][i];
            int textureIndex = (tileNumber == 1) ? 1 : 0;
            int tu = textureIndex % (m_tileset.getSize().x / tileSize.x);
            int tv = textureIndex / (m_tileset.getSize().x / tileSize.x);
            sf::Vertex* quad = &m_vertices[(i + j * width) * 4];
            quad[0].position = sf::Vector2f(i * tileSize.x, j * tileSize.y);
            quad[1].position = sf::Vector2f((i + 1) * tileSize.x, j * tileSize.y);
            quad[2].position = sf::Vector2f((i + 1) * tileSize.x, (j + 1) * tileSize.y);
            quad[3].position = sf::Vector2f(i * tileSize.x, (j + 1) * tileSize.y);
            quad[0].texCoords = sf::Vector2f(tu * tileSize.x, tv * tileSize.y);
            quad[1].texCoords = sf::Vector2f((tu + 1) * tileSize.x, tv * tileSize.y);
            quad[2].texCoords = sf::Vector2f((tu + 1) * tileSize.x, (tv + 1) * tileSize.y);
            quad[3].texCoords = sf::Vector2f(tu * tileSize.x, (tv + 1) * tileSize.y);
        }
    }
    return true;
}
void Tilemap::draw(sf::RenderTarget& target, sf::RenderStates states) const {
    states.transform *= getTransform();
    states.texture = &m_tileset;
    target.draw(m_vertices, states);
}
