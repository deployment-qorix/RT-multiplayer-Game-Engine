#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <stdexcept>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

constexpr int MAX_PLAYERS = 16;
constexpr int MAX_CHAT_MESSAGE_LENGTH = 128;
constexpr int TCP_PORT = 1337;
constexpr int UDP_PORT = 1338;
constexpr int GAME_VERSION = 1;

// ---------------- Message Types ----------------
enum class MessageType : uint8_t {
    Handshake,
    HandshakeResult,
    PlayerJoin,
    PlayerLeave,
    PlayerState,
    AllPlayersState,
    PlayerInput,
    PlayerShoot,
    ProjectileSpawn,
    PlayerHit,
    PlayerRespawn,
    GameStateUpdate,
    ClientReady,
    ChatMessage
};

enum class GameState : uint8_t {
    LOBBY,
    IN_PROGRESS,
    GAME_OVER
};

// ---------------- Data Structures ----------------
struct HandshakeData {
    uint32_t version;
};

struct HandshakeResultData {
    bool success;
    char message[MAX_CHAT_MESSAGE_LENGTH];
};

struct PlayerInputData {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    glm::quat rotation = glm::quat(1, 0, 0, 0);
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

struct AllPlayersStateData {
    int count;
    PlayerStateData players[MAX_PLAYERS];
};

struct ProjectileData {
    glm::vec3 start_position;
    glm::vec3 direction;
};

struct PlayerHitData {
    uint32_t victim_id;
    uint32_t attacker_id;
    int new_health;
};

struct PlayerRespawnData {
    uint32_t player_id;
    glm::vec3 position;
};

struct GameStateData {
    GameState state;
    uint32_t winner_id;
};

struct ClientReadyData { };

struct ChatMessageData {
    uint32_t player_id;
    char text[MAX_CHAT_MESSAGE_LENGTH];
};

struct PlayerShootData { };

// ---------------- GameMessage Wrapper ----------------
struct GameMessage {
    MessageType type;
    char data[2048]; // increased buffer size

    // ---- Generic Set / Get helpers ----
    template<typename T>
    void setData(const T& d) {
        static_assert(sizeof(T) <= sizeof(data), "Data too large for GameMessage::data");
        std::memcpy(data, &d, sizeof(T));
    }

    template<typename T>
    T getData() const {
        T d;
        std::memcpy(&d, data, sizeof(T));
        return d;
    }

    // ---- Serialize to raw buffer ----
    std::vector<char> serialize() const {
        std::vector<char> buffer(sizeof(MessageType) + sizeof(data));
        std::memcpy(buffer.data(), &type, sizeof(MessageType));
        std::memcpy(buffer.data() + sizeof(MessageType), data, sizeof(data));
        return buffer;
    }

    // ---- Deserialize from raw buffer ----
    static GameMessage deserialize(const char* buffer) {
        GameMessage msg;
        std::memcpy(&msg.type, buffer, sizeof(MessageType));
        std::memcpy(msg.data, buffer + sizeof(MessageType), sizeof(msg.data));
        return msg;
    }
};

// ---------------- UDP Message ----------------
struct UDPMessage {
    uint32_t player_id;
    glm::vec3 position;
    glm::quat rotation;
};
