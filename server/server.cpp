#include "../shared/protocol.h"
#include <iostream>
#include <memory>
#include <map>
#include <mutex>
#include <chrono>
#include <boost/asio.hpp>
#include <glm/gtc/epsilon.hpp>
#include <random>
#include <vector>
#include <set>

using boost::asio::ip::tcp;

class Game;

struct AABB {
    glm::vec3 min;
    glm::vec3 max;
};

struct PlayerState {
    uint32_t id;
    glm::vec3 position{0.0f, 0.0f, 3.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    AABB bounding_box;
    int health = 100;
    std::chrono::steady_clock::time_point death_timestamp;
    int kills = 0;
    int deaths = 0;
    bool is_ready = false;

    void update_bounding_box() {
        glm::vec3 size(0.5f, 1.0f, 0.5f); 
        bounding_box.min = position - size;
        bounding_box.max = position + size;
    }
};

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, Game& game);
    void start();
    void deliver(const GameMessage& msg);
    uint32_t get_player_id() const { return player_id_; }
private:
    void do_read();
    tcp::socket socket_;
    Game& game_;
    GameMessage read_msg_;
    uint32_t player_id_;
    static uint32_t next_player_id_;
};
uint32_t Session::next_player_id_ = 1;

class Game {
public:
    Game();
    void join(std::shared_ptr<Session> participant);
    void leave(std::shared_ptr<Session> participant);
    void process_message(const GameMessage& msg, uint32_t sender_id);
    void update();
private:
    void startGame();
    void resetMatch();
    bool CheckCollision(const AABB& a, const AABB& b);
    void broadcast(const GameMessage& msg);
    std::map<uint32_t, std::shared_ptr<Session>> sessions_;
    std::map<uint32_t, PlayerState> player_states_;
    std::mutex mutex_;
    const float PLAYER_SPEED = 0.1f;
    const int RESPAWN_DELAY_SECONDS = 5;
    const int KILLS_TO_WIN = 5;
    const int MATCH_RESTART_DELAY_SECONDS = 10;

    GameState game_state_ = GameState::LOBBY;
    std::chrono::steady_clock::time_point match_over_timestamp_;

    std::mt19937 random_generator_;
    std::uniform_int_distribution<int> spawn_distribution_;
    
    std::vector<AABB> static_colliders_;
};

Game::Game() : spawn_distribution_(-10, 10) {
    std::random_device rd;
    random_generator_.seed(rd());

    static_colliders_.push_back({{-20.0f, -1.5f, -20.0f}, {20.0f, -0.5f, 20.0f}});
    static_colliders_.push_back({{-5.0f, -0.5f, -5.0f}, {-3.0f, 1.5f, -3.0f}});
    static_colliders_.push_back({{3.0f, -0.5f, 4.0f}, {5.0f, 1.5f, 6.0f}});
    static_colliders_.push_back({{-2.0f, -0.5f, 8.0f}, {2.0f, 0.5f, 9.0f}});
}

Session::Session(tcp::socket socket, Game& game) : socket_(std::move(socket)), game_(game) {
    player_id_ = next_player_id_++;
}
void Session::start() { game_.join(shared_from_this()); do_read(); }
void Session::deliver(const GameMessage& msg) {
    boost::asio::async_write(socket_, boost::asio::buffer(&msg, sizeof(GameMessage)),
        [self = shared_from_this()](boost::system::error_code ec, std::size_t) {
            if (ec) { self->game_.leave(self); }
        });
}
void Session::do_read() {
    auto self(shared_from_this());
    boost::asio::async_read(socket_, boost::asio::buffer(&read_msg_, sizeof(GameMessage)),
        [this, self](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                game_.process_message(read_msg_, self->get_player_id());
                do_read();
            } else {
                game_.leave(self);
            }
        });
}

void Game::startGame(){
    std::cout << "--- ALL PLAYERS READY! STARTING GAME --- \n";
    game_state_ = GameState::IN_PROGRESS;
    resetMatch();
}

void Game::resetMatch(){
    std::cout << "--- RESETTING MATCH --- \n";
    for(auto& [id, state] : player_states_){
        state.health = 100;
        state.kills = 0;
        state.deaths = 0;
        state.position = glm::vec3(spawn_distribution_(random_generator_), 0.0f, spawn_distribution_(random_generator_));
        state.update_bounding_box();
    }
    game_state_ = GameState::IN_PROGRESS;

    GameMessage reset_msg;
    reset_msg.type = MessageType::GameStateUpdate;
    reset_msg.data.game_state_data = {game_state_, 0};
    broadcast(reset_msg);
}

void Game::update() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sessions_.empty()) return;

    auto now = std::chrono::steady_clock::now();

    if (game_state_ == GameState::LOBBY) {
        if (!player_states_.empty()) {
            bool all_ready = true;
            for (const auto& [id, state] : player_states_) {
                if (!state.is_ready) {
                    all_ready = false;
                    break;
                }
            }
            if (all_ready) {
                startGame();
            }
        }
    }
    else if (game_state_ == GameState::GAME_OVER) {
        auto time_since_game_over = std::chrono::duration_cast<std::chrono::seconds>(now - match_over_timestamp_).count();
        if (time_since_game_over >= MATCH_RESTART_DELAY_SECONDS) {
            game_state_ = GameState::LOBBY;
            for(auto& pair : player_states_) pair.second.is_ready = false;
            
            GameMessage lobby_msg;
            lobby_msg.type = MessageType::GameStateUpdate;
            lobby_msg.data.game_state_data = {game_state_, 0};
            broadcast(lobby_msg);
        }
    }
    else if(game_state_ == GameState::IN_PROGRESS){
        for (auto& [id, state] : player_states_) {
            if (state.health <= 0) {
                auto time_since_death = std::chrono::duration_cast<std::chrono::seconds>(now - state.death_timestamp).count();
                if (time_since_death >= RESPAWN_DELAY_SECONDS) {
                    state.health = 100;
                    state.position = glm::vec3(spawn_distribution_(random_generator_), 0.0f, spawn_distribution_(random_generator_));
                    state.update_bounding_box();
                }
            } else {
                 state.position.y -= 0.05f;
                state.update_bounding_box();
                for(const auto& collider : static_colliders_){
                    if(CheckCollision(state.bounding_box, collider)){
                        state.position.y += 0.05f;
                        state.update_bounding_box();
                        break;
                    }
                }
            }
        }
    }

    GameMessage state_msg;
    state_msg.type = MessageType::PlayerState;
    state_msg.data.all_players_state_data.count = player_states_.size();
    
    int i = 0;
    for (const auto& [id, state] : player_states_) {
        state_msg.data.all_players_state_data.players[i++] = {state.id, state.position, state.rotation, state.bounding_box.min, state.bounding_box.max, state.health, state.kills, state.deaths, state.is_ready};
    }
    broadcast(state_msg);
}

void Game::join(std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t id = session->get_player_id();
    sessions_[id] = session;
    
    PlayerState new_player;
    new_player.id = id;
    new_player.position = glm::vec3(spawn_distribution_(random_generator_), 0.0f, spawn_distribution_(random_generator_));
    new_player.update_bounding_box();
    player_states_[id] = new_player;
    
    std::cout << "Player " << id << " joined.\n";
    
    const auto& new_player_state = player_states_[id];
    
    GameMessage spawn_msg;
    spawn_msg.type = MessageType::PlayerJoin;
    spawn_msg.data.player_join_data = {id, new_player_state.position, new_player_state.rotation, new_player_state.bounding_box.min, new_player_state.bounding_box.max, new_player_state.health, new_player_state.kills, new_player_state.deaths, new_player_state.is_ready};
    broadcast(spawn_msg);
    
    for (const auto& [other_id, state] : player_states_) {
        if (id == other_id) continue;
        GameMessage existing_player_msg;
        existing_player_msg.type = MessageType::PlayerJoin;
        existing_player_msg.data.player_join_data = {state.id, state.position, state.rotation, state.bounding_box.min, state.bounding_box.max, state.health, state.kills, state.deaths, state.is_ready};
        session->deliver(existing_player_msg);
    }

    GameMessage current_game_state_msg;
    current_game_state_msg.type = MessageType::GameStateUpdate;
    current_game_state_msg.data.game_state_data = {game_state_, 0};
    session->deliver(current_game_state_msg);
}

void Game::leave(std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t id = session->get_player_id();
    sessions_.erase(id);
    player_states_.erase(id);
    std::cout << "Player " << id << " left.\n";
    GameMessage leave_msg;
    leave_msg.type = MessageType::PlayerLeave;
    leave_msg.data.player_leave_id = id;
    broadcast(leave_msg);
}

bool Game::CheckCollision(const AABB& a, const AABB& b) {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

void Game::process_message(const GameMessage& msg, uint32_t sender_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (player_states_.find(sender_id) == player_states_.end()) return;
    
    if (msg.type == MessageType::ChatMessage) {
        std::cout << "Player " << sender_id << " says: " << msg.data.chat_message_data.text << "\n";
        broadcast(msg);
        return;
    }

    if (game_state_ == GameState::LOBBY && msg.type == MessageType::ClientReady) {
        player_states_.at(sender_id).is_ready = true;
        std::cout << "Player " << sender_id << " is ready.\n";
        return;
    }

    if (game_state_ != GameState::IN_PROGRESS) return;
    if (player_states_.at(sender_id).health <= 0) return;

    if (msg.type == MessageType::PlayerInput) {
        PlayerState& state = player_states_[sender_id];
        state.rotation = msg.data.player_input_data.rotation;
        
        glm::vec3 original_position = state.position;
        glm::vec3 forward = state.rotation * glm::vec3(0, 0, -1);
        glm::vec3 right = state.rotation * glm::vec3(1, 0, 0);

        if (msg.data.player_input_data.up) state.position += forward * PLAYER_SPEED;
        if (msg.data.player_input_data.down) state.position -= forward * PLAYER_SPEED;
        if (msg.data.player_input_data.left) state.position -= right * PLAYER_SPEED;
        if (msg.data.player_input_data.right) state.position += right * PLAYER_SPEED;
        
        state.update_bounding_box();

        for (const auto& [other_id, other_state] : player_states_) {
            if (sender_id == other_id || other_state.health <= 0) continue;
            if (CheckCollision(state.bounding_box, other_state.bounding_box)) {
                state.position = original_position;
                state.update_bounding_box();
                return;
            }
        }
        
        for (const auto& collider : static_colliders_){
             if(CheckCollision(state.bounding_box, collider)){
                 state.position = original_position;
                 state.update_bounding_box();
                 return;
             }
        }
    } 
    else if (msg.type == MessageType::PlayerShoot) {
        PlayerState& shooter = player_states_[sender_id];
        glm::vec3 forward = shooter.rotation * glm::vec3(0, 0, -1);

        GameMessage projectile_msg;
        projectile_msg.type = MessageType::ProjectileSpawn;
        projectile_msg.data.projectile_spawn_data = {shooter.position, forward};
        broadcast(projectile_msg);

        for (auto& [target_id, target_state] : player_states_) {
            if (sender_id == target_id || target_state.health <= 0) continue;
            glm::vec3 dir_to_target = glm::normalize(target_state.position - shooter.position);
            
            if (glm::dot(forward, dir_to_target) > 0.95f) {
                if(glm::distance(shooter.position, target_state.position) < 50.0f) {
                    target_state.health -= 25;
                    if (target_state.health <= 0) {
                        target_state.health = 0;
                        target_state.death_timestamp = std::chrono::steady_clock::now();
                        target_state.deaths++;
                        shooter.kills++;
                        
                        if(shooter.kills >= KILLS_TO_WIN){
                            game_state_ = GameState::GAME_OVER;
                            match_over_timestamp_ = std::chrono::steady_clock::now();
                            GameMessage end_msg;
                            end_msg.type = MessageType::GameStateUpdate;
                            end_msg.data.game_state_data = {game_state_, sender_id};
                            broadcast(end_msg);
                        }
                    }
                    break;
                }
            }
        }
    }
}
void Game::broadcast(const GameMessage& msg) {
    for (const auto& [id, session] : sessions_) {
        session->deliver(msg);
    }
}

class Server {
public:
    Server(boost::asio::io_context& io_context, short port, Game& game)
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), game_(game), timer_(io_context, std::chrono::milliseconds(16)) {
        do_accept();
        run_game_update();
    }
private:
    void run_game_update() {
        game_.update();
        timer_.expires_at(timer_.expiry() + std::chrono::milliseconds(16));
        timer_.async_wait([this](const boost::system::error_code&){ run_game_update(); });
    }
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) { std::make_shared<Session>(std::move(socket), game_)->start(); }
                do_accept();
            });
    }
    tcp::acceptor acceptor_;
    Game& game_;
    boost::asio::steady_timer timer_;
};

int main() {
    try {
        boost::asio::io_context io_context;
        Game game;
        Server server(io_context, 1337, game);
        io_context.run();
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}

