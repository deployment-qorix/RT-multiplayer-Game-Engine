#include "Game.h"
#include <iostream>
#include <vector>
#include <cmath>
#include "imgui.h"
#include "imgui-SFML.h"

const float TILE_SIZE = 18.0f;
const int MAP_WIDTH_TILES = 20;

Game::Game() 
    : m_window(sf::VideoMode(1280, 720), "Pac-Man Client") {
    m_view.setSize(MAP_WIDTH_TILES * TILE_SIZE, 21 * TILE_SIZE);
    m_view.setCenter((MAP_WIDTH_TILES * TILE_SIZE) / 2.f, (21 * TILE_SIZE) / 2.f);
    m_window.setFramerateLimit(60);
    if (!ImGui::SFML::Init(m_window)) throw std::runtime_error("Failed to init ImGui");
    try {
        m_networkClient.connect("127.0.0.1", 1337);
    } catch(const std::exception& e) {
        throw std::runtime_error("Connection failed: " + std::string(e.what()));
    }
    if (!m_font.loadFromFile("../assets/font.ttf")) {
        throw std::runtime_error("Failed to load font!");
    }
    if (!m_playerTexture.loadFromFile("../assets/player.png") ||
        !m_pelletTexture.loadFromFile("../assets/pellet.png") ||
        !m_ghostTexture.loadFromFile("../assets/ghost.png")) {
        throw std::runtime_error("Failed to load assets!");
    }
    std::vector<std::vector<int>> mapLayout;
    if (!m_tilemap.load("../assets/tileset.png", sf::Vector2u(18, 18), "../assets/map.txt", mapLayout)) {
        throw std::runtime_error("Failed to load tilemap!");
    }
    int mapWidth = mapLayout.empty() ? 0 : mapLayout[0].size();
    for (size_t j = 0; j < mapLayout.size(); ++j) {
        for (size_t i = 0; i < mapLayout[j].size(); ++i) {
            if (mapLayout[j][i] == 0) {
                int key = j * mapWidth + i;
                m_pellets[key].setTexture(m_pelletTexture);
                m_pellets[key].setPosition(i * TILE_SIZE, j * TILE_SIZE);
            }
        }
    }
}
void Game::run() {
    sf::Clock deltaClock;
    while (m_window.isOpen()) {
        processEvents();
        update(deltaClock.restart());
        render();
    }
    ImGui::SFML::Shutdown();
}
void Game::processEvents() {
    sf::Event event;
    while (m_window.pollEvent(event)) {
        ImGui::SFML::ProcessEvent(m_window, event);
        if (event.type == sf::Event::Closed) m_window.close();
        if (event.type == sf::Event::KeyPressed && event.key.code == sf::Keyboard::Escape) {
            m_window.close();
        }
    }
}
void Game::update(sf::Time deltaTime) {
    ImGui::SFML::Update(m_window, deltaTime);
    handleNetworkMessages();
    handleInput();
    float interp_speed = 10.0f;
    float dt = deltaTime.asSeconds();
    auto interpolate = [&](float& visual, int server) {
        float target = server * TILE_SIZE;
        float diff = target - visual;
        if (std::abs(diff) < 0.1f) {
            visual = target;
        } else {
            visual += diff * interp_speed * dt;
        }
    };
    for (auto& [id, player] : m_players) {
        interpolate(player.visual_x, player.server_x);
        interpolate(player.visual_y, player.server_y);
        if (player.animation_timer.getElapsedTime() > sf::milliseconds(100)) {
            player.current_frame = (player.current_frame + 1) % 2;
            player.animation_timer.restart();
        }
    }
    for (auto& [id, ghost] : m_ghosts) {
        interpolate(ghost.visual_x, ghost.server_x);
        interpolate(ghost.visual_y, ghost.server_y);
    }
    if (m_myPlayerId != 0 && m_players.count(m_myPlayerId)) {
        m_view.setCenter(m_players[m_myPlayerId].visual_x, m_players[m_myPlayerId].visual_y);
    }
}
void Game::drawText(const std::string& str, int size, sf::Color color) {
    sf::Text text(str, m_font, size);
    sf::FloatRect textBounds = text.getLocalBounds();
    text.setOrigin(textBounds.left + textBounds.width / 2.0f, textBounds.top + textBounds.height / 2.0f);
    text.setPosition(m_window.getSize().x / 2.0f, m_window.getSize().y / 2.0f);
    text.setFillColor(color);
    m_window.draw(text);
}
void Game::render() {
    m_window.clear(sf::Color::Black);
    if (m_currentState == GameState::PLAYING) {
        m_window.setView(m_view);
        m_window.draw(m_tilemap);
        for (const auto& [key, pellet] : m_pellets) {
            m_window.draw(pellet);
        }
        for (auto& [id, ghost] : m_ghosts) {
            ghost.sprite.setPosition(ghost.visual_x, ghost.visual_y);
            m_window.draw(ghost.sprite);
        }
        for (auto& [id, player] : m_players) {
            int frame_width = player.sprite.getTexture()->getSize().x / 2;
            int frame_height = player.sprite.getTexture()->getSize().y;
            player.sprite.setTextureRect(sf::IntRect(player.current_frame * frame_width, 0, frame_width, frame_height));
            player.sprite.setRotation(player.rotation);
            player.sprite.setPosition(player.visual_x, player.visual_y);
            m_window.draw(player.sprite);
        }
    }
    m_window.setView(m_window.getDefaultView());
    if (m_currentState == GameState::START_SCREEN) {
        drawText("Press a key to start", 50, sf::Color::White);
    } else if (m_currentState == GameState::GAME_OVER) {
        drawText("GAME OVER", 80, sf::Color::Red);
    } else if (m_currentState == GameState::YOU_WIN) {
        drawText("YOU WIN!", 80, sf::Color::Green);
    }
    ImGui::Begin("Game Info");
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    if (m_myPlayerId != 0 && m_players.count(m_myPlayerId)) {
        ImGui::Text("Score: %d", m_players[m_myPlayerId].score);
        ImGui::Text("Lives: %d", m_players[m_myPlayerId].lives);
    }
    ImGui::End();
    ImGui::SFML::Render(m_window);
    m_window.display();
}
void Game::handleNetworkMessages() {
    for (int i = 0; i < 10 && m_networkClient.has_messages(); ++i) {
        GameMessage msg = m_networkClient.pop_message();
        switch (msg.type) {
            case MessageType::GameStateUpdate:
                m_currentState = msg.data.game_state_data.state;
                break;
            case MessageType::PlayerSpawn: {
                uint32_t id = msg.data.player_data.player_id;
                m_players[id].sprite.setTexture(m_playerTexture);
                m_players[id].sprite.setOrigin(m_playerTexture.getSize().x / 4.f, m_playerTexture.getSize().y / 2.f);
                m_players[id].server_x = msg.data.player_data.x;
                m_players[id].server_y = msg.data.player_data.y;
                m_players[id].visual_x = msg.data.player_data.x * TILE_SIZE;
                m_players[id].visual_y = msg.data.player_data.y * TILE_SIZE;
                m_players[id].score = msg.data.player_data.score;
                m_players[id].lives = msg.data.player_data.lives;
                if(m_myPlayerId == 0) m_myPlayerId = id;
                break;
            }
            case MessageType::PlayerPosition: {
                uint32_t id = msg.data.player_data.player_id;
                if(m_players.count(id)) {
                    float old_x = m_players[id].server_x;
                    float old_y = m_players[id].server_y;
                    m_players[id].server_x = msg.data.player_data.x;
                    m_players[id].server_y = msg.data.player_data.y;
                    m_players[id].lives = msg.data.player_data.lives;
                    float dx = m_players[id].server_x - old_x;
                    float dy = m_players[id].server_y - old_y;
                    if (dx > 0) m_players[id].rotation = 90.f;
                    else if (dx < 0) m_players[id].rotation = 270.f;
                    else if (dy > 0) m_players[id].rotation = 180.f;
                    else if (dy < 0) m_players[id].rotation = 0.f;
                }
                break;
            }
            case MessageType::PlayerDespawn:
                m_players.erase(msg.data.id_to_destroy);
                break;
            case MessageType::UpdateScore: {
                uint32_t id = msg.data.player_data.player_id;
                if(m_players.count(id)) {
                    m_players[id].score = msg.data.player_data.score;
                }
                break;
            }
            case MessageType::PelletCollected: {
                int mapWidth = 20;
                int key = msg.data.pellet_data.y * mapWidth + msg.data.pellet_data.x;
                m_pellets.erase(key);
                break;
            }
            case MessageType::GhostUpdate: {
                for (int j = 0; j < msg.data.all_ghosts_data.count; ++j) {
                    const auto& ghost_data = msg.data.all_ghosts_data.ghosts[j];
                    uint32_t id = ghost_data.id;
                    if (m_ghosts.find(id) == m_ghosts.end()) {
                        m_ghosts[id].sprite.setTexture(m_ghostTexture);
                        m_ghosts[id].visual_x = ghost_data.x * TILE_SIZE;
                        m_ghosts[id].visual_y = ghost_data.y * TILE_SIZE;
                    }
                    m_ghosts[id].server_x = ghost_data.x;
                    m_ghosts[id].server_y = ghost_data.y;
                }
                break;
            }
        }
    }
}
void Game::handleInput() {
    if (m_inputClock.getElapsedTime() > sf::milliseconds(150)) {
        PlayerInputData input;
        bool has_input = false;
        if (sf::Keyboard::isKeyPressed(sf::Keyboard::W)) { input.up = true; has_input = true; }
        else if (sf::Keyboard::isKeyPressed(sf::Keyboard::S)) { input.down = true; has_input = true; }
        else if (sf::Keyboard::isKeyPressed(sf::Keyboard::A)) { input.left = true; has_input = true; }
        else if (sf::Keyboard::isKeyPressed(sf::Keyboard::D)) { input.right = true; has_input = true; }
        if (has_input) {
            GameMessage msg;
            msg.type = MessageType::PlayerInput;
            msg.data.input_data = input;
            m_networkClient.send(msg);
            m_inputClock.restart();
        }
    }
}
