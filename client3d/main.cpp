#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include "include/shader.h"
#include "include/camera.h"
#include "include/model.h"
#include <iostream>
#include <thread>
#include <map>
#include <cstdio>
#include <vector>
#include <list>
#include <string>
#include "NetworkClient.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <SFML/Audio.hpp>

// Function prototypes
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow *window, PlayerInputData& input, NetworkClient& client, sf::Sound& shootSound, const GameState& gameState);

// Constants and globals
const unsigned int SCR_WIDTH = 1280;
const unsigned int SCR_HEIGHT = 720;
Camera camera(glm::vec3(0.0f, 2.0f, 8.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;
bool show_cursor = false;
bool chat_input_active = false;
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Struct definitions
struct AABB { glm::vec3 min; glm::vec3 max; };
struct Player {
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 visual_position;
    AABB bounding_box;
    int health;
    int kills;
    int deaths;
    bool is_ready;
};
struct StaticObject { glm::vec3 position; glm::vec3 scale; glm::vec3 color; };
struct Projectile {
    glm::vec3 position;
    glm::vec3 direction;
    float speed = 100.0f;
    float lifetime = 1.0f;
};

int main() {
    // Initialization
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "3D Client", NULL, NULL);
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glEnable(GL_DEPTH_TEST);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Asset Loading
    Shader ourShader("shaders/1.model_loading.vs", "shaders/1.model_loading.fs");
    Model ourModel("assets/models/backpack/backpack.obj");
    Shader debugShader("shaders/debug.vs", "shaders/debug.fs");
    sf::SoundBuffer shootBuffer;
    if (!shootBuffer.loadFromFile("assets/sounds/shoot.wav")) { std::cerr << "Error: Could not load shoot.wav\n"; }
    sf::Sound shootSound;
    shootSound.setBuffer(shootBuffer);
    sf::SoundBuffer hitBuffer;
    if (!hitBuffer.loadFromFile("assets/sounds/hit.wav")) { std::cerr << "Error: Could not load hit.wav\n"; }
    sf::Sound hitSound;
    hitSound.setBuffer(hitBuffer);
    
    // Vertex Data
    float vertices[] = { -0.5f,-0.5f,-0.5f, 0.5f,-0.5f,-0.5f, 0.5f, 0.5f,-0.5f, 0.5f, 0.5f,-0.5f,-0.5f, 0.5f,-0.5f,-0.5f,-0.5f,-0.5f, -0.5f,-0.5f, 0.5f, 0.5f,-0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,-0.5f, 0.5f, 0.5f,-0.5f,-0.5f, 0.5f, -0.5f, 0.5f, 0.5f,-0.5f, 0.5f,-0.5f,-0.5f, 0.5f,-0.5f,-0.5f, 0.5f,-0.5f,-0.5f, 0.5f,-0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,-0.5f, 0.5f,-0.5f,-0.5f,-0.5f,-0.5f,-0.5f,-0.5f,-0.5f, 0.5f, 0.5f,-0.5f, 0.5f, 0.5f, 0.5f,-0.5f, 0.5f, -0.5f,-0.5f,-0.5f, 0.5f,-0.5f,-0.5f, 0.5f,-0.5f, 0.5f, 0.5f,-0.5f, 0.5f,-0.5f,-0.5f, 0.5f,-0.5f,-0.5f,-0.5f, -0.5f, 0.5f,-0.5f, 0.5f, 0.5f,-0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f,-0.5f, 0.5f, 0.5f,-0.5f, 0.5f,-0.5f };
    unsigned int worldVAO, worldVBO;
    glGenVertexArrays(1, &worldVAO); glGenBuffers(1, &worldVBO);
    glBindVertexArray(worldVAO); glBindBuffer(GL_ARRAY_BUFFER, worldVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0); glBindVertexArray(0);
    unsigned int projectileVAO, projectileVBO;
    glGenVertexArrays(1, &projectileVAO); glGenBuffers(1, &projectileVBO);

    // Game World
    std::vector<StaticObject> static_objects;
    static_objects.push_back({ {0.0f, -1.0f, 0.0f}, {40.0f, 1.0f, 40.0f}, {0.2f, 0.8f, 0.2f} });
    static_objects.push_back({ {-4.0f, 0.5f, -4.0f}, {2.0f, 2.0f, 2.0f}, {0.8f, 0.2f, 0.2f} });
    static_objects.push_back({ {4.0f, 0.5f, 5.0f}, {2.0f, 2.0f, 2.0f}, {0.2f, 0.2f, 0.8f} });
    static_objects.push_back({ {0.0f, 0.0f, 8.5f}, {4.0f, 1.0f, 1.0f}, {0.8f, 0.8f, 0.2f} });

    // Network
    boost::asio::io_context io_context;
    NetworkClient client(io_context);
    client.connect("127.0.0.1", std::to_string(TCP_PORT));
    std::thread network_thread([&io_context](){ io_context.run(); });

    // Game State
    uint32_t my_player_id = 0;
    std::map<uint32_t, Player> server_player_states;
    Player my_predicted_state;
    my_predicted_state.position = glm::vec3(0.0f, 0.0f, 3.0f);
    my_predicted_state.visual_position = my_predicted_state.position;
    auto last_input_send_time = std::chrono::steady_clock::now();
    int my_last_health = 100;
    std::list<Projectile> projectiles;
    GameState current_game_state = GameState::LOBBY;
    uint32_t winner_id = 0;
    char chat_input_buf[MAX_CHAT_MESSAGE_LENGTH] = "";
    std::vector<std::string> chat_history;

    // Game Loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame; lastFrame = currentFrame;

        PlayerInputData current_input = {};
        processInput(window, current_input, client, shootSound, current_game_state);

        if (current_game_state == GameState::IN_PROGRESS && my_player_id != 0 && server_player_states.count(my_player_id) && server_player_states.at(my_player_id).health > 0) {
            const float PLAYER_SPEED = 2.5f;
            my_predicted_state.rotation = camera.getRotationQuat();
            glm::vec3 forward = my_predicted_state.rotation * glm::vec3(0, 0, -1);
            glm::vec3 right = my_predicted_state.rotation * glm::vec3(1, 0, 0);
            if (current_input.up) my_predicted_state.position += forward * PLAYER_SPEED * deltaTime;
            if (current_input.down) my_predicted_state.position -= forward * PLAYER_SPEED * deltaTime;
            if (current_input.left) my_predicted_state.position -= right * PLAYER_SPEED * deltaTime;
            if (current_input.right) my_predicted_state.position += right * PLAYER_SPEED * deltaTime;
        }
        
        { 
            std::lock_guard<std::mutex> lock(client.incoming_mutex);
            // Process reliable TCP messages
            while (!client.incoming_tcp_messages.empty()) {
                GameMessage msg = client.incoming_tcp_messages.front();
                client.incoming_tcp_messages.pop_front();
                
                if (msg.type == MessageType::PlayerJoin && my_player_id == 0) {
                    const auto& join_data = msg.getData<PlayerStateData>();
                    my_player_id = join_data.id;
                    client.set_id(my_player_id);
                }
                
                switch (msg.type) {
                    case MessageType::PlayerJoin: {
                        const auto& join_data = msg.getData<PlayerStateData>();
                        server_player_states[join_data.id] = { join_data.position, join_data.rotation, join_data.position, {join_data.box_min, join_data.box_max}, join_data.health, join_data.kills, join_data.deaths, join_data.is_ready};
                        break;
                    }
                    case MessageType::PlayerLeave: {
                        uint32_t id = msg.getData<uint32_t>();
                        server_player_states.erase(id);
                        break;
                    }
                    case MessageType::PlayerState: {
                        const auto& all_states_data = msg.getData<AllPlayersStateData>();
                        for (int i = 0; i < all_states_data.count; ++i) {
                            const auto& state_data = all_states_data.players[i];
                            if (server_player_states.count(state_data.id)) {
                                if (state_data.id != my_player_id) { 
                                    server_player_states[state_data.id].position = state_data.position;
                                    server_player_states[state_data.id].rotation = state_data.rotation;
                                }
                                server_player_states[state_data.id].bounding_box.min = state_data.box_min;
                                server_player_states[state_data.id].bounding_box.max = state_data.box_max;
                                server_player_states[state_data.id].health = state_data.health;
                                server_player_states[state_data.id].kills = state_data.kills;
                                server_player_states[state_data.id].deaths = state_data.deaths;
                                server_player_states[state_data.id].is_ready = state_data.is_ready;
                            }
                        }
                        break;
                    }
                    case MessageType::ProjectileSpawn: {
                        const auto& spawn_data = msg.getData<ProjectileData>();
                        projectiles.push_back({spawn_data.start_position, spawn_data.direction});
                        break;
                    }
                    case MessageType::GameStateUpdate: {
                        const auto& state_data = msg.getData<GameStateData>();
                        current_game_state = state_data.state;
                        winner_id = state_data.winner_id;
                        if(current_game_state == GameState::IN_PROGRESS || current_game_state == GameState::LOBBY){
                            projectiles.clear();
                        }
                        break;
                    }
                    case MessageType::PlayerHit: {
                        const auto& hit_data = msg.getData<PlayerHitData>();
                        if (server_player_states.count(hit_data.victim_id)) {
                            server_player_states.at(hit_data.victim_id).health = hit_data.new_health;
                        }
                        hitSound.play();
                        break;
                    }
                    case MessageType::PlayerRespawn: {
                        const auto& respawn_data = msg.getData<PlayerRespawnData>();
                        if (server_player_states.count(respawn_data.player_id)) {
                            server_player_states.at(respawn_data.player_id).health = 100;
                            server_player_states.at(respawn_data.player_id).position = respawn_data.position;
                        }
                        break;
                    }
                    case MessageType::ChatMessage: {
                        const auto& chat_data = msg.getData<ChatMessageData>();
                        std::string chat_msg = "Player " + std::to_string(chat_data.player_id) + ": " + chat_data.text;
                        chat_history.push_back(chat_msg);
                        if(chat_history.size() > 10) chat_history.erase(chat_history.begin());
                        break;
                    }
                    default: break;
                }
            }
            
            // Process fast UDP messages
            while (!client.incoming_udp_messages.empty()) {
                UDPMessage msg = client.incoming_udp_messages.front();
                client.incoming_udp_messages.pop_front();
                
                if (server_player_states.count(msg.player_id)) {
                    if (msg.player_id != my_player_id) {
                        server_player_states.at(msg.player_id).position = msg.position;
                        server_player_states.at(msg.player_id).rotation = msg.rotation;
                    } else {
                        // This is the server's authoritative position for our own player.
                        // We can use this for server reconciliation to correct our prediction.
                        server_player_states.at(my_player_id).position = msg.position;
                    }
                }
            }
        }
        
        if (my_player_id != 0 && server_player_states.count(my_player_id)) { 
            my_predicted_state.position = server_player_states.at(my_player_id).position; 
        }
        my_predicted_state.visual_position = glm::mix(my_predicted_state.visual_position, my_predicted_state.position, 15.0f * deltaTime);
        for(auto& pair : server_player_states){
            if(pair.first != my_player_id){
                pair.second.visual_position = glm::mix(pair.second.visual_position, pair.second.position, 15.0f * deltaTime);
            }
        }
        for (auto it = projectiles.begin(); it != projectiles.end(); ) {
            it->position += it->direction * it->speed * deltaTime;
            it->lifetime -= deltaTime;
            if (it->lifetime <= 0) { it = projectiles.erase(it); } else { ++it; }
        }
        
        auto now = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - last_input_send_time).count() > 50) {
            GameMessage input_msg;
            input_msg.type = MessageType::PlayerInput;
            current_input.rotation = camera.getRotationQuat();
            input_msg.setData(current_input);
            client.send_tcp(input_msg);
            last_input_send_time = now;
        }
        
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame();
        
        if (current_game_state == GameState::LOBBY) {
            show_cursor = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            ImGui::SetNextWindowPos(ImVec2(SCR_WIDTH * 0.5f, SCR_HEIGHT * 0.5f), ImGuiCond_Always, ImVec2(0.5,0.5));
            ImGui::SetNextWindowSize(ImVec2(400,300), ImGuiCond_Always);
            ImGui::Begin("Lobby", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
            ImGui::Text("Waiting for players (2 needed to start)...");
            ImGui::Separator();
            if (ImGui::BeginTable("lobby_players", 2, ImGuiTableFlags_Borders)) {
                ImGui::TableSetupColumn("Player ID"); ImGui::TableSetupColumn("Status");
                ImGui::TableHeadersRow();
                for(const auto& [id, player] : server_player_states){
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0); ImGui::Text("%u", id);
                    ImGui::TableSetColumnIndex(1);
                    if(player.is_ready) { ImGui::TextColored(ImVec4(0,1,0,1), "Ready"); } 
                    else { ImGui::Text("Not Ready"); }
                }
                ImGui::EndTable();
            }
            ImGui::Separator();
            if (server_player_states.count(my_player_id) && !server_player_states.at(my_player_id).is_ready) {
            if (ImGui::Button("Ready Up", ImVec2(-1, 40))) {
                GameMessage ready_msg;
                ready_msg.type = MessageType::ClientReady;
                ready_msg.setData(ClientReadyData{});
                client.send_tcp(ready_msg);
                }
                ImGui::Text("Waiting for other players to ready up...");
            }

            ImGui::End();
        } else {
            if (!chat_input_active) {
                show_cursor = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            if(my_player_id != 0 && server_player_states.count(my_player_id)){
                glm::vec3 player_visual_pos = my_predicted_state.visual_position;
                float distance_from_player = 5.0f;
                camera.Position = player_visual_pos - (camera.Front * distance_from_player) + glm::vec3(0.0, 1.5, 0.0);
            }
            glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
            glm::mat4 view = camera.GetViewMatrix();
            ourShader.use();
            ourShader.setMat4("projection", projection);
            ourShader.setMat4("view", view);
            glm::vec3 lightPos(0.0f, 10.0f, 10.0f);
            ourShader.setVec3("viewPos", camera.Position);
            ourShader.setVec3("light.position", lightPos);
            ourShader.setVec3("light.ambient", 0.2f, 0.2f, 0.2f);
            ourShader.setVec3("light.diffuse", 0.8f, 0.8f, 0.8f);
            ourShader.setVec3("light.specular", 1.0f, 1.0f, 1.0f);
            ourShader.setFloat("material.shininess", 32.0f);
            glBindVertexArray(worldVAO);
            for(const auto& object : static_objects){
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, object.position);
                model = glm::scale(model, object.scale);
                ourShader.setMat4("model", model);
                ourShader.setVec3("objectColor", object.color);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
            for (const auto& [id, state] : server_player_states) {
                if (state.health <= 0) continue;
                glm::mat4 model = glm::mat4(1.0f);
                if (id == my_player_id) {
                    model = glm::translate(model, my_predicted_state.visual_position);
                    model = model * glm::mat4_cast(camera.getRotationQuat());
                } else {
                    model = glm::translate(model, state.visual_position);
                    model = model * glm::mat4_cast(state.rotation);
                }
                ourShader.setMat4("model", model);
                ourModel.Draw(ourShader);
            }
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            debugShader.use();
            debugShader.setMat4("projection", projection);
            debugShader.setMat4("view", view);
            debugShader.setVec3("color", 0.0f, 1.0f, 0.0f);
            for (const auto& [id, state] : server_player_states) {
                if (state.health <= 0) continue;
                glm::vec3 size = state.bounding_box.max - state.bounding_box.min;
                glm::vec3 center = state.bounding_box.min + size * 0.5f;
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::translate(model, center);
                model = glm::scale(model, size);
                debugShader.setMat4("model", model);
                glBindVertexArray(worldVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            debugShader.use();
            debugShader.setMat4("projection", projection);
            debugShader.setMat4("view", view);
            debugShader.setVec3("color", 1.0f, 1.0f, 0.0f);
            glLineWidth(3.0f);
            for (const auto& p : projectiles) {
                glm::vec3 end_pos = p.position + p.direction * 0.5f;
                float line_verts[] = { p.position.x, p.position.y, p.position.z, end_pos.x, end_pos.y, end_pos.z };
                glBindVertexArray(projectileVAO);
                glBindBuffer(GL_ARRAY_BUFFER, projectileVBO);
                glBufferData(GL_ARRAY_BUFFER, sizeof(line_verts), line_verts, GL_DYNAMIC_DRAW);
                glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
                glEnableVertexAttribArray(0);
                glDrawArrays(GL_LINES, 0, 2);
            }
            glLineWidth(1.0f);
            glBindVertexArray(0);
        }
        ImGui::SetNextWindowPos(ImVec2(SCR_WIDTH - 260, 10), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(250, 150), ImGuiCond_Always);
        ImGui::Begin("Scoreboard", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        if (ImGui::BeginTable("scores", 3, ImGuiTableFlags_Borders)) {
            ImGui::TableSetupColumn("Player ID"); ImGui::TableSetupColumn("Kills"); ImGui::TableSetupColumn("Deaths");
            ImGui::TableHeadersRow();
            for(const auto& [id, player] : server_player_states) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%u", id);
                ImGui::TableSetColumnIndex(1); ImGui::Text("%d", player.kills);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%d", player.deaths);
            }
            ImGui::EndTable();
        }
        ImGui::End();
        ImGui::SetNextWindowPos(ImVec2(10, SCR_HEIGHT - 160), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(400, 150), ImGuiCond_Always);
        ImGui::Begin("Chat", NULL, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::BeginChild("ChatHistory", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
        for(const auto& msg : chat_history){ ImGui::TextUnformatted(msg.c_str()); }
        ImGui::EndChild();
        ImGui::PushItemWidth(-1);
        if(chat_input_active) ImGui::SetKeyboardFocusHere();
        if(ImGui::InputText("##chat", chat_input_buf, MAX_CHAT_MESSAGE_LENGTH, ImGuiInputTextFlags_EnterReturnsTrue)){
            if(strlen(chat_input_buf) > 0){
                GameMessage msg;
                msg.type = MessageType::ChatMessage;
                ChatMessageData chat_data;
                chat_data.player_id = my_player_id;
                strncpy(chat_data.text, chat_input_buf, MAX_CHAT_MESSAGE_LENGTH);
                msg.setData(chat_data);
                client.send_tcp(msg);
                strcpy(chat_input_buf, "");
            }
            chat_input_active = false;
        }
        if(ImGui::IsWindowFocused()){ chat_input_active = true; }
        ImGui::PopItemWidth();
        ImGui::End();
        if(server_player_states.count(my_player_id)) {
            if(server_player_states.at(my_player_id).health <= 0 && current_game_state == GameState::IN_PROGRESS){
                ImGui::SetNextWindowPos(ImVec2(SCR_WIDTH * 0.5f, SCR_HEIGHT * 0.5f), ImGuiCond_Always, ImVec2(0.5,0.5));
                ImGui::SetNextWindowSize(ImVec2(400,100));
                ImGui::Begin("Eliminated", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
                ImGui::Text("\n           YOU ARE ELIMINATED!");
                ImGui::Text("           Respawning soon...");
                ImGui::End();
            }
        }
        if(current_game_state == GameState::GAME_OVER){
            ImGui::SetNextWindowPos(ImVec2(SCR_WIDTH * 0.5f, SCR_HEIGHT * 0.5f), ImGuiCond_Always, ImVec2(0.5,0.5));
            ImGui::SetNextWindowSize(ImVec2(400,120));
            ImGui::Begin("Game Over", NULL, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
            ImGui::Text("\n              GAME OVER!");
            ImGui::Text("           Player %u is the winner!", winner_id);
            ImGui::Text("\n         Returning to lobby soon...");
            ImGui::End();
        }
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // Cleanup
    client.close();
    if(network_thread.joinable()) { network_thread.join(); }
    glfwTerminate();
    return 0;
}

void processInput(GLFWwindow *window, PlayerInputData& input, NetworkClient& client, sf::Sound& shootSound, const GameState& gameState) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
    static bool chat_key_pressed = false;
    if(glfwGetKey(window, GLFW_KEY_T) == GLFW_PRESS && !chat_key_pressed && !chat_input_active){
        chat_input_active = true; show_cursor = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        chat_key_pressed = true;
    }
    if(glfwGetKey(window, GLFW_KEY_T) == GLFW_RELEASE) { chat_key_pressed = false; }
    if (chat_input_active) return;
    if (gameState != GameState::IN_PROGRESS) {
        if (!show_cursor) { show_cursor = true; glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL); }
        return;
    }
    if (glfwGetKey(window, GLFW_KEY_TAB) == GLFW_PRESS) {
        show_cursor = !show_cursor;
        glfwSetInputMode(window, GLFW_CURSOR, show_cursor ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
    }
    static bool shoot_key_pressed = false;
    if (glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS) {
        if (!shoot_key_pressed) {
            GameMessage msg;
            msg.type = MessageType::PlayerShoot;
            msg.setData(PlayerShootData{});
            client.send_tcp(msg);
            shootSound.play();
            shoot_key_pressed = true;
        }
    } else { shoot_key_pressed = false; }
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { input.up = true; }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { input.down = true; }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { input.left = true; }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { input.right = true; }
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) { glViewport(0, 0, width, height); }

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (show_cursor || chat_input_active) return;
    if (firstMouse) { lastX = (float)xpos; lastY = (float)ypos; firstMouse = false; }
    float xoffset = (float)xpos - lastX;
    float yoffset = lastY - (float)ypos;
    lastX = (float)xpos; lastY = (float)ypos;
    camera.ProcessMouseMovement(xoffset, yoffset);
}

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) { camera.ProcessMouseScroll((float)yoffset); }