#pragma once
#include "NetworkClient.h"
#include "Tilemap.h"
#include "protocol.h"
#include <SFML/Graphics.hpp>
#include <map>

struct Player {
    sf::Sprite sprite;
    int server_x, server_y;
    float visual_x, visual_y;
    int score, lives;
    float rotation = 0.0f;
    int current_frame = 0;
    sf::Clock animation_timer;
};
struct Ghost {
    sf::Sprite sprite;
    int server_x, server_y;
    float visual_x, visual_y;
};
enum class GameState {
    START_SCREEN,
    PLAYING,
    GAME_OVER,
    YOU_WIN
};


class Game {
public:
    Game();
    void run();
private:
    void processEvents();
    void update(sf::Time deltaTime);
    void render();
    void handleNetworkMessages();
    void handleInput();
    void drawText(const std::string& str, int size, sf::Color color);
    GameState m_currentState = GameState::START_SCREEN;
    sf::RenderWindow m_window;
    sf::View m_view;
    NetworkClient m_networkClient;
    Tilemap m_tilemap;
    sf::Font m_font;
    sf::Texture m_playerTexture;
    sf::Texture m_pelletTexture, m_ghostTexture;
    std::map<uint32_t, Player> m_players;
    std::map<int, sf::Sprite> m_pellets;
    std::map<uint32_t, Ghost> m_ghosts;
    uint32_t m_myPlayerId = 0;
    sf::Clock m_inputClock;
};