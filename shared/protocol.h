#pragma once

#include <cstdint>

// A constant for the maximum number of ghosts in the game.
const int MAX_GHOSTS = 4;

// Defines the overall state of the game.
enum class GameState : uint8_t {
    START_SCREEN,
    PLAYING,
    GAME_OVER,
    YOU_WIN
};

// Defines the different types of messages that can be sent between the client and server.
enum class MessageType : uint8_t {
    PlayerInput,      // Client->Server: A player's movement input.
    GameStateUpdate,  // Server->Client: A change in the global game state.
    PlayerSpawn,      // Server->Client: A player has appeared in the game.
    PlayerPosition,   // Server->Client: An update on a player's position and status.
    PlayerDespawn,    // Server->Client: A player has been removed from the game.
    UpdateScore,      // Server->Client: A player's score has changed.
    PelletCollected,  // Server->Client: A pellet has been eaten and should be removed.
    GhostUpdate       // Server->Client: A snapshot of all ghost positions.
};

// Data for a GameStateUpdate message.
struct GameStateData {
    GameState state;
};

// Data for PlayerSpawn, PlayerPosition, and UpdateScore messages.
struct PlayerData {
    uint32_t player_id;
    int x;
    int y;
    int score;
    int lives;
};

// Data for a PelletCollected message.
struct PelletData {
    int x;
    int y;
};

// Data for a single ghost.
struct GhostData {
    uint32_t id;
    int x;
    int y;
};

// A snapshot of all ghosts for the GhostUpdate message.
struct AllGhostsData {
    uint8_t count;
    GhostData ghosts[MAX_GHOSTS];
};

// Data for a PlayerInput message.
struct PlayerInputData {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
};

// The main message structure that is sent over the network.
// It contains the message type and a union of all possible data payloads.
struct GameMessage {
    GameMessage() : data{} {} // Default constructor to initialize the union
    MessageType type;
    union {
        GameStateData   game_state_data;
        PlayerData      player_data;
        uint32_t        id_to_destroy;
        PelletData      pellet_data;
        AllGhostsData   all_ghosts_data;
        PlayerInputData input_data;
    } data;
};