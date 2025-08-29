#pragma once
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

const int MAX_PLAYERS = 16;

enum class MessageType : uint8_t {
    PlayerJoin,
    PlayerLeave,
    PlayerState,
    PlayerInput,
    PlayerShoot // NEW: Client->Server message when a player shoots
};

struct PlayerStateData {
    uint32_t id;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 box_min;
    glm::vec3 box_max;
    int health; // NEW: Player health
};

struct AllPlayersStateData {
    uint8_t count;
    PlayerStateData players[MAX_PLAYERS];
};

struct PlayerInputData {
    bool up, down, left, right;
    glm::quat rotation;
};

// This is just a tag, it carries no data
struct PlayerShootData {};

struct GameMessage {
    GameMessage() : data{} {}
    MessageType type;
    union {
        PlayerStateData      player_join_data;
        uint32_t             player_leave_id;
        AllPlayersStateData  all_players_state_data;
        PlayerInputData      player_input_data;
        PlayerShootData      player_shoot_data;
    } data;
};

