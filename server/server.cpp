#include "protocol.h"
#include <iostream>
#include <memory>
#include <map>
#include <mutex>
#include <chrono>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class Game;

struct PlayerState {
    uint32_t id;
    glm::vec3 position{0.0f, 0.0f, 3.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
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
    void join(std::shared_ptr<Session> participant);
    void leave(std::shared_ptr<Session> participant);
    void process_message(const GameMessage& msg, uint32_t sender_id);
    void update();
private:
    void broadcast(const GameMessage& msg);
    std::map<uint32_t, std::shared_ptr<Session>> sessions_;
    std::map<uint32_t, PlayerState> player_states_;
    std::mutex mutex_;
    const float PLAYER_SPEED = 0.1f;
};

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

void Game::update() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (sessions_.empty()) return;

    GameMessage state_msg;
    state_msg.type = MessageType::PlayerState;
    state_msg.data.all_players_state_data.count = player_states_.size();
    
    int i = 0;
    for (const auto& [id, state] : player_states_) {
        state_msg.data.all_players_state_data.players[i++] = {state.id, state.position, state.rotation};
    }
    broadcast(state_msg);
}

void Game::join(std::shared_ptr<Session> session) {
    std::lock_guard<std::mutex> lock(mutex_);
    uint32_t id = session->get_player_id();
    sessions_[id] = session;
    player_states_[id] = PlayerState{id};
    std::cout << "Player " << id << " joined.\n";
    
    const auto& new_player_state = player_states_[id];
    
    // Tell everyone about the new player
    GameMessage spawn_msg;
    spawn_msg.type = MessageType::PlayerJoin;
    spawn_msg.data.player_join_data = {id, new_player_state.position, new_player_state.rotation};
    broadcast(spawn_msg);
    
    // Tell the new player about everyone else
    for (const auto& [other_id, state] : player_states_) {
        if (id == other_id) continue;
        GameMessage existing_player_msg;
        existing_player_msg.type = MessageType::PlayerJoin;
        existing_player_msg.data.player_join_data = {state.id, state.position, state.rotation};
        session->deliver(existing_player_msg);
    }
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

void Game::process_message(const GameMessage& msg, uint32_t sender_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (player_states_.find(sender_id) == player_states_.end()) return;

    if (msg.type == MessageType::PlayerInput) {
        PlayerState& state = player_states_[sender_id];
        state.rotation = msg.data.player_input_data.rotation;
        
        glm::vec3 forward = state.rotation * glm::vec3(0, 0, -1);
        glm::vec3 right = state.rotation * glm::vec3(1, 0, 0);

        if (msg.data.player_input_data.up) state.position += forward * PLAYER_SPEED;
        if (msg.data.player_input_data.down) state.position -= forward * PLAYER_SPEED;
        if (msg.data.player_input_data.left) state.position -= right * PLAYER_SPEED;
        if (msg.data.player_input_data.right) state.position += right * PLAYER_SPEED;
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
        : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), game_(game), timer_(io_context, std::chrono::milliseconds(16)) { // ~60 ticks
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