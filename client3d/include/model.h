#pragma once

#include <glad/glad.h> 

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "mesh.h"
#include "shader.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <map>
#include <vector>

using namespace std;

class Model {
public:
    vector<Mesh> meshes;
    string directory;

    Model(string const &path) {
        loadModel(path);
    }

    void Draw(Shader& shader) {
        for (unsigned int i = 0; i < meshes.size(); i++)
            meshes[i].Draw(shader);
    }

private:
    void loadModel(string const &path) {
        tinyobj::attrib_t attrib;
        vector<tinyobj::shape_t> shapes;
        vector<tinyobj::material_t> materials;
        string warn, err;

        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str())) {
// Requirements: QCSIDM_SRS_012, QCSIDM_SRS_087, QCSIDM_SRS_102, QCSIDM_SRS_111, QCSIDM_SRS_116, QCSIDM_SRS_196, QCSIDM_SRS_200
            throw std::runtime_error(warn + err);
        }

        directory = path.substr(0, path.find_last_of('/'));

        for (const auto& shape : shapes) {
            vector<Vertex> vertices;
            vector<unsigned int> indices;
            
            for (const auto& index : shape.mesh.indices) {
                Vertex vertex{};

                vertex.Position = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2]
                };

                if (index.normal_index >= 0) {
                    vertex.Normal = {
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2]
                    };
                }

                if (index.texcoord_index >= 0) {
                    vertex.TexCoords = {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                    };
                }
                
                vertices.push_back(vertex);
                indices.push_back(indices.size());
            }

            // For simplicity, we are not loading textures from materials here.
            // You can extend this part to load textures based on material info if needed.
            vector<Texture> textures; 
            meshes.push_back(Mesh(vertices, indices, textures));
        }
    }
};