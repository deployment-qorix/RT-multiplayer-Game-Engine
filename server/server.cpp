// server.cpp (cleaned)
// Compatible with your provided protocol.h and client code (main.cpp / NetworkClient.cpp)

#include <boost/asio.hpp>
#include <iostream>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <deque>
#include <chrono>
#include <random>
#include <vector>
#include <cstring>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include "../shared/protocol.h"

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

// ---------------------- Serialization helpers (must match client) ----------------------

static std::vector<char> serialize_game_message(const GameMessage& m) {
    // Body = 4-byte type + 256 bytes payload
    std::vector<char> body(sizeof(uint32_t) + sizeof(m.data));
    uint32_t t = static_cast<uint32_t>(m.type);
    std::memcpy(body.data(), &t, sizeof(uint32_t));
    std::memcpy(body.data() + sizeof(uint32_t), &m.data, sizeof(m.data));
    return body;
}

static GameMessage deserialize_game_message(const std::vector<char>& body) {
    if (body.size() != sizeof(uint32_t) + 256)
        throw std::runtime_error("invalid TCP body size");
    GameMessage m{};
    uint32_t t = 0;
    std::memcpy(&t, body.data(), sizeof(uint32_t));
    m.type = static_cast<MessageType>(t);
    std::memcpy(&m.data, body.data() + sizeof(uint32_t), 256);
    return m;
}

static std::vector<char> serialize_udp_message(const UDPMessage& u) {
    std::vector<char> buf(sizeof(UDPMessage));
    std::memcpy(buf.data(), &u, sizeof(UDPMessage));
    return buf;
}

static UDPMessage deserialize_udp_message(const char* data, std::size_t len) {
    if (len != sizeof(UDPMessage)) throw std::runtime_error("invalid UDP size");
    UDPMessage u{};
    std::memcpy(&u, data, sizeof(UDPMessage));
    return u;
}

// ---------------------- Game data structures ----------------------

struct AABB { glm::vec3 min; glm::vec3 max; };

struct PlayerRuntime {
    uint32_t id = 0;
    glm::vec3 position{0.0f, 0.0f, 3.0f};
    glm::quat rotation{1, 0, 0, 0};
    AABB box{};
    int health = 100;
    int kills = 0;
    int deaths = 0;
    bool ready = false;

    std::chrono::steady_clock::time_point death_time{};

    void update_aabb() {
        // Match your client’s debug bbox ~ 1x2x1 around center
        glm::vec3 half(0.5f, 1.0f, 0.5f);
        box.min = position - half;
        box.max = position + half;
    }
};

// Forward declarations
class Game;
class Session;

// ---------------------- Session: one TCP client ----------------------

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, Game& game) : socket_(std::move(socket)), game_(game) {}

    void start();
    void deliver(const GameMessage& msg); // thread-safe: called from Game under lock

    uint32_t id() const { return id_; }
    void set_id(uint32_t v) { id_ = v; }

private:
    void read_header();
    void read_body(std::size_t body_len);
    void write_next();

    tcp::socket socket_;
    Game& game_;

    // TCP length-prefixed header (4 bytes)
    enum { header_len = sizeof(uint32_t) };
    std::array<char, header_len> header_buf_{};
    std::vector<char> body_buf_;

    std::deque<std::vector<char>> write_q_;
    uint32_t id_ = 0;
};

// ---------------------- Game core ----------------------

class Game {
public:
    explicit Game(boost::asio::io_context& io)
        : io_(io),
          acceptor_(io, tcp::endpoint(tcp::v4(), TCP_PORT)),
          udp_socket_(io, udp::endpoint(udp::v4(), UDP_PORT)),
          tick_(io, std::chrono::milliseconds(16)),
          rng_(std::random_device{}()),
          spawn_rng_(-10, 10) {
        // Simple world colliders like your client scene
        colliders_.push_back({{ -20.f, -1.5f, -20.f }, { 20.f, -0.5f, 20.f }}); // "ground slab"
        colliders_.push_back({{ -5.f, -0.5f, -5.f },  { -3.f,  1.5f, -3.f }});   // red box
        colliders_.push_back({{  3.f, -0.5f,  4.f },  {  5.f,  1.5f,  6.f }});   // blue box

        do_accept();
        do_receive_udp();
        tick_loop();
    }

    // Session lifecycle
    void join(const std::shared_ptr<Session>& s);
    void leave(const std::shared_ptr<Session>& s);

    // Incoming gameplay messages
    void handle_msg(uint32_t sender_id, const GameMessage& msg);

    // UDP send helper
    void send_udp_to(uint32_t player_id, const UDPMessage& m);

private:
    // Loop
    void tick_loop();

    // Networking
    void do_accept();
    void do_receive_udp();

    // Broadcasts (caller must hold lock)
    void broadcast(const GameMessage& msg);
    void send_to(uint32_t id, const GameMessage& msg);

    // Game helpers
    bool aabb_overlap(const AABB& a, const AABB& b) const;
    void start_match();
    void reset_match();

private:
    boost::asio::io_context& io_;
    tcp::acceptor acceptor_;
    udp::socket udp_socket_;
    udp::endpoint udp_remote_;
    std::array<char, 1024> udp_buf_{};

    boost::asio::steady_timer tick_;

    std::mutex mtx_;
    std::unordered_map<uint32_t, std::shared_ptr<Session>> sessions_;
    std::unordered_map<uint32_t, PlayerRuntime> players_;
    std::unordered_map<uint32_t, udp::endpoint> udp_eps_;

    GameState state_ = GameState::LOBBY;
    uint32_t next_id_ = 1;
    std::chrono::steady_clock::time_point gameover_time_{};

    // World
    std::vector<AABB> colliders_;

    // RNG for spawn points
    std::mt19937 rng_;
    std::uniform_int_distribution<int> spawn_rng_;
};

// ====================== Session implementation ======================

void Session::start() { read_header(); }

void Session::deliver(const GameMessage& msg) {
    auto body = serialize_game_message(msg);

    bool writing = !write_q_.empty();
    write_q_.push_back(std::move(body));
    if (!writing) write_next();
}

void Session::read_header() {
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(header_buf_.data(), header_len),
        [this, self](boost::system::error_code ec, std::size_t /*n*/) {
            if (ec) { game_.leave(self); return; }
            uint32_t body_len = 0;
            std::memcpy(&body_len, header_buf_.data(), sizeof(uint32_t));
            if (body_len == 0 || body_len > 4096) { game_.leave(self); return; }
            body_buf_.resize(body_len);
            read_body(body_len);
        }
    );
}

void Session::read_body(std::size_t body_len) {
    auto self = shared_from_this();
    boost::asio::async_read(
        socket_,
        boost::asio::buffer(body_buf_.data(), body_len),
        [this, self](boost::system::error_code ec, std::size_t /*n*/) {
            if (ec) { game_.leave(self); return; }
            try {
                GameMessage m = deserialize_game_message(body_buf_);
                game_.handle_msg(id_, m);
            } catch (...) {
                // bad packet, drop client
                game_.leave(self);
                return;
            }
            read_header();
        }
    );
}

void Session::write_next() {
    if (write_q_.empty()) return;

    // Build [header][body] buffers
    const auto& body = write_q_.front();
    uint32_t len = static_cast<uint32_t>(body.size());
    std::array<char, sizeof(uint32_t)> hdr{};
    std::memcpy(hdr.data(), &len, sizeof(uint32_t));
    std::vector<boost::asio::const_buffer> bufs;
    bufs.emplace_back(boost::asio::buffer(hdr));
    bufs.emplace_back(boost::asio::buffer(body));

    auto self = shared_from_this();
    boost::asio::async_write(
        socket_,
        bufs,
        [this, self](boost::system::error_code ec, std::size_t /*n*/) {
            if (ec) { game_.leave(self); return; }
            write_q_.pop_front();
            if (!write_q_.empty()) write_next();
        }
    );
}

// ====================== Game implementation ======================

void Game::do_accept() {
    acceptor_.async_accept([this](boost::system::error_code ec, tcp::socket sock) {
        if (!ec) {
            auto s = std::make_shared<Session>(std::move(sock), *this);
            {
                std::lock_guard<std::mutex> lock(mtx_);
                // Reserve ID; we’ll finalize join() to broadcast
                s->set_id(next_id_++);
                sessions_[s->id()] = s;
            }
            s->start();

            // Complete join immediately (send PlayerJoin etc.)
            join(s);
        }
        do_accept();
    });
}

void Game::join(const std::shared_ptr<Session>& s) {
    std::lock_guard<std::mutex> lock(mtx_);

    // Create player runtime
    PlayerRuntime p{};
    p.id = s->id();
    p.health = 100;
    p.kills = 0;
    p.deaths = 0;
    p.ready = false;
    p.position = glm::vec3(static_cast<float>(spawn_rng_(rng_)), 0.0f, static_cast<float>(spawn_rng_(rng_)));
    p.rotation = glm::quat(1, 0, 0, 0);
    p.update_aabb();
    players_[p.id] = p;

    std::cout << "[Server] Player " << p.id << " joined.\n";

    // 1) Tell the joining client about THEMSELVES first (so client sets my_player_id correctly)
    {
        GameMessage msg{};
        msg.type = MessageType::PlayerJoin;
        PlayerStateData d{ p.id, p.position, p.rotation, p.box.min, p.box.max, p.health, p.kills, p.deaths, p.ready };
        msg.setData(d);
        send_to(p.id, msg);
    }

    // 2) Tell the joining client about all other existing players
    for (const auto& [oid, op] : players_) {
        if (oid == p.id) continue;
        GameMessage msg{};
        msg.type = MessageType::PlayerJoin;
        PlayerStateData d{ op.id, op.position, op.rotation, op.box.min, op.box.max, op.health, op.kills, op.deaths, op.ready };
        msg.setData(d);
        send_to(p.id, msg);
    }

    // 3) Tell everyone else about the new player
    {
        GameMessage msg{};
        msg.type = MessageType::PlayerJoin;
        PlayerStateData d{ p.id, p.position, p.rotation, p.box.min, p.box.max, p.health, p.kills, p.deaths, p.ready };
        msg.setData(d);
        for (const auto& [sid, sess] : sessions_) {
            if (sid == p.id) continue;
            sess->deliver(msg);
        }
    }

    // 4) Send current game state to the joining client
    {
        GameMessage gs{};
        gs.type = MessageType::GameStateUpdate;
        GameStateData gsd{ state_, 0 };
        gs.setData(gsd);
        send_to(p.id, gs);
    }
}

void Game::leave(const std::shared_ptr<Session>& s) {
    std::lock_guard<std::mutex> lock(mtx_);
    const auto pid = s->id();
    if (!sessions_.erase(pid)) return;

    players_.erase(pid);
    udp_eps_.erase(pid);

    GameMessage msg{};
    msg.type = MessageType::PlayerLeave;
    msg.setData(pid);
    broadcast(msg);

    std::cout << "[Server] Player " << pid << " left.\n";
}

void Game::handle_msg(uint32_t sender_id, const GameMessage& msg) {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!players_.count(sender_id)) return;

    auto& st = players_.at(sender_id);

    switch (msg.type) {
        case MessageType::ClientReady: {
            st.ready = true;
            std::cout << "[Server] Player " << sender_id << " ready.\n";
        } break;

        case MessageType::ChatMessage: {
            // Just relay as-is
            auto chat = msg.getData<ChatMessageData>();
            broadcast(msg);
            std::cout << "[Chat] Player " << chat.player_id << ": " << chat.text << "\n";
        } break;

        case MessageType::PlayerInput: {
            if (state_ != GameState::IN_PROGRESS || st.health <= 0) break;
            auto in = msg.getData<PlayerInputData>();
            st.rotation = in.rotation;

            // movement
            glm::vec3 f = st.rotation * glm::vec3(0, 0, -1);
            glm::vec3 r = st.rotation * glm::vec3(1, 0, 0);
            glm::vec3 old = st.position;

            constexpr float kStep = 0.1f; // server step per input packet
            if (in.up)    st.position += f * kStep;
            if (in.down)  st.position -= f * kStep;
            if (in.left)  st.position -= r * kStep;
            if (in.right) st.position += r * kStep;

            st.update_aabb();

            // collide players
            for (auto& [oid, other] : players_) {
                if (oid == sender_id || other.health <= 0) continue;
                if (aabb_overlap(st.box, other.box)) {
                    st.position = old;
                    st.update_aabb();
                    break;
                }
            }
            // collide world
            for (const auto& c : colliders_) {
                if (aabb_overlap(st.box, c)) {
                    st.position = old;
                    st.update_aabb();
                    break;
                }
            }
        } break;

        case MessageType::PlayerShoot: {
            if (state_ != GameState::IN_PROGRESS || st.health <= 0) break;

            glm::vec3 f = st.rotation * glm::vec3(0, 0, -1);

            // Broadcast projectile spawn (for visuals)
            {
                GameMessage proj{};
                proj.type = MessageType::ProjectileSpawn;
                ProjectileData pd{ st.position, f };
                proj.setData(pd);
                broadcast(proj);
            }

            // Very simple hitscan: dot and distance check
            for (auto& [tid, target] : players_) {
                if (tid == sender_id || target.health <= 0) continue;

                glm::vec3 diff = target.position - st.position;
                float dist2 = glm::dot(diff, diff);
                if (dist2 > 50.0f * 50.0f) continue;

                glm::vec3 dir_to_target = glm::normalize(diff);
                if (glm::dot(f, dir_to_target) > 0.95f) {
                    target.health -= 25;
                    GameMessage hit{};
                    hit.type = MessageType::PlayerHit;
                    PlayerHitData hd{ tid, target.health };
                    hit.setData(hd);
                    broadcast(hit);

                    if (target.health <= 0) {
                        target.deaths++;
                        st.kills++;
                        target.death_time = std::chrono::steady_clock::now();

                        // Win condition: first to 5 kills
                        if (st.kills >= 5) {
                            state_ = GameState::GAME_OVER;
                            gameover_time_ = std::chrono::steady_clock::now();
                            GameMessage end{};
                            end.type = MessageType::GameStateUpdate;
                            GameStateData ed{ state_, sender_id };
                            end.setData(ed);
                            broadcast(end);
                        }
                    }
                }
            }
        } break;

        default:
            // ignore unknown/unused here
            break;
    }
}

void Game::tick_loop() {
    // Game tick @ ~60Hz
    {
        std::lock_guard<std::mutex> lock(mtx_);

        auto now = std::chrono::steady_clock::now();

        if (state_ == GameState::LOBBY) {
            if (players_.size() > 1) {
                bool all_ready = true;
                for (auto& [id, p] : players_) {
                    if (!p.ready) { all_ready = false; break; }
                }
                if (all_ready) start_match();
            }
        } else if (state_ == GameState::GAME_OVER) {
            if (std::chrono::duration_cast<std::chrono::seconds>(now - gameover_time_).count() >= 10) {
                state_ = GameState::LOBBY;
                for (auto& [id, p] : players_) p.ready = false;

                GameMessage gs{};
                gs.type = MessageType::GameStateUpdate;
                GameStateData sd{ state_, 0 };
                gs.setData(sd);
                broadcast(gs);
            }
        } else if (state_ == GameState::IN_PROGRESS) {
            // Handle respawns
            for (auto& [id, p] : players_) {
                if (p.health <= 0) {
                    if (std::chrono::duration_cast<std::chrono::seconds>(now - p.death_time).count() >= 5) {
                        p.health = 100;
                        p.position = glm::vec3(static_cast<float>(spawn_rng_(rng_)), 0.0f,
                                               static_cast<float>(spawn_rng_(rng_)));
                        p.update_aabb();

                        GameMessage resp{};
                        resp.type = MessageType::PlayerRespawn;
                        PlayerRespawnData rd{ id, p.position };
                        resp.setData(rd);
                        broadcast(resp);
                    }
                }
            }
        }

        // Send periodic PlayerState (All players) over TCP so clients stay in sync even without UDP
        AllPlayersStateData batch{};
        batch.count = 0;
        for (auto& [id, p] : players_) {
            if (batch.count >= MAX_PLAYERS) break;
            PlayerStateData d{ p.id, p.position, p.rotation, p.box.min, p.box.max, p.health, p.kills, p.deaths, p.ready };
            batch.players[batch.count++] = d;
        }
        GameMessage ps{};
        ps.type = MessageType::PlayerState;
        ps.setData(batch);
        broadcast(ps);

        // Also stream UDP position/rotation if endpoint registered
        for (auto& [id, p] : players_) {
            if (!udp_eps_.count(id)) continue;
            UDPMessage u{ id, p.position, p.rotation };
            send_udp_to(id, u);
        }
    }

    tick_.expires_after(std::chrono::milliseconds(16));
    tick_.async_wait([this](const boost::system::error_code&) { tick_loop(); });
}

void Game::do_receive_udp() {
    udp_socket_.async_receive_from(
        boost::asio::buffer(udp_buf_), udp_remote_,
        [this](boost::system::error_code ec, std::size_t bytes) {
            if (!ec) {
                try {
                    auto u = deserialize_udp_message(udp_buf_.data(), bytes);
                    std::lock_guard<std::mutex> lock(mtx_);
                    // Register/refresh endpoint for this player_id
                    udp_eps_[u.player_id] = udp_remote_;
                } catch (...) {
                    // ignore bad UDP packets
                }
            }
            do_receive_udp();
        }
    );
}

void Game::send_udp_to(uint32_t player_id, const UDPMessage& m) {
    auto it = udp_eps_.find(player_id);
    if (it == udp_eps_.end()) return;
    auto buf = serialize_udp_message(m);
    udp_socket_.async_send_to(
        boost::asio::buffer(buf),
        it->second,
        [](boost::system::error_code, std::size_t) {});
}

void Game::broadcast(const GameMessage& msg) {
    for (auto& [id, s] : sessions_) s->deliver(msg);
}

void Game::send_to(uint32_t id, const GameMessage& msg) {
    auto it = sessions_.find(id);
    if (it != sessions_.end()) it->second->deliver(msg);
}

bool Game::aabb_overlap(const AABB& a, const AABB& b) const {
    return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
           (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
           (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

void Game::start_match() {
    state_ = GameState::IN_PROGRESS;
    reset_match();
    GameMessage gs{};
    gs.type = MessageType::GameStateUpdate;
    GameStateData sd{ state_, 0 };
    gs.setData(sd);
    broadcast(gs);
    std::cout << "[Server] Match started.\n";
}

void Game::reset_match() {
    for (auto& [id, p] : players_) {
        p.health = 100;
        p.kills = 0;
        p.deaths = 0;
        p.position = glm::vec3(static_cast<float>(spawn_rng_(rng_)), 0.0f,
                               static_cast<float>(spawn_rng_(rng_)));
        p.update_aabb();
    }
}

int main() {
    try {
        boost::asio::io_context io;
        Game game(io);
        std::cout << "[Server] Running on TCP " << TCP_PORT << " UDP " << UDP_PORT << "\n";
        io.run();
    } catch (const std::exception& e) {
        std::cerr << "Server exception: " << e.what() << "\n";
        return 1;
    }
}
