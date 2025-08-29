#pragma once
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

const int MAX_PLAYERS = 16;
const int MAX_CHAT_MESSAGE_LENGTH = 128; // NEW

enum class GameState : uint8_t {
    LOBBY,
    IN_PROGRESS,
    GAME_OVER
};

enum class MessageType : uint8_t {
    PlayerJoin,
    PlayerLeave,
    PlayerState,
    PlayerInput,
    PlayerShoot,
    ProjectileSpawn,
    GameStateUpdate,
    ClientReady,
    ChatMessage // NEW: For sending chat messages
};

struct PlayerStateData {
    uint32_t id;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 box_min;
    glm::vec3 box_max;
    int health;
    int kills;
    int deaths;
    bool is_ready;
};

struct GameStateData {
    GameState state;
    uint32_t winner_id;
};

// NEW: Data for a chat message
struct ChatMessageData {
    uint32_t player_id;
    char text[MAX_CHAT_MESSAGE_LENGTH];
};

struct AllPlayersStateData {
    uint8_t count;
    PlayerStateData players[MAX_PLAYERS];
};

struct PlayerInputData {
    bool up, down, left, right;
    glm::quat rotation;
};

struct PlayerShootData {};
struct ClientReadyData {};

struct ProjectileData {
    glm::vec3 start_position;
    glm::vec3 direction;
};

struct GameMessage {
    GameMessage() : data{} {}
    MessageType type;
    union {
        PlayerStateData      player_join_data;
        uint32_t             player_leave_id;
        AllPlayersStateData  all_players_state_data;
        PlayerInputData      player_input_data;
        PlayerShootData      player_shoot_data;
        ProjectileData       projectile_spawn_data;
        GameStateData        game_state_data;
        ClientReadyData      client_ready_data;
        ChatMessageData      chat_message_data; // NEW
    } data;
};

