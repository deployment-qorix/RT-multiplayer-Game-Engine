#pragma once
#include <SFML/Graphics.hpp>
#include <string>
#include <vector>
class Tilemap : public sf::Drawable, public sf::Transformable {
public:
    bool load(const std::string& tileset, sf::Vector2u tileSize, const std::string& mapFile, std::vector<std::vector<int>>& layout);
private:
    virtual void draw(sf::RenderTarget& target, sf::RenderStates states) const;
    sf::VertexArray m_vertices;
    sf::Texture m_tileset;
};