/*
 * readScene.h
 *
 *  Created on: 29.05.2021
 *      Author: farnsworth
 */

#ifndef SRC_READSCENE_H_
#define SRC_READSCENE_H_

#include <iostream>
#include <vector>
#include <string>
#include <stack>

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/gtx/string_cast.hpp>

struct Camera {
	glm::vec3 eye;
	glm::vec3 center;
	glm::vec3 worldUp;
	float fovDeg;		 // field of view in degrees

	int width;
	int height;

	glm::vec3 u, v, w; // camera frame axes

	// update/calculate camera frame axes
	void updateAxes() {
		glm::vec3 eyeDir = center - eye;
		w = glm::normalize(-eyeDir);
		u = glm::normalize(glm::cross(worldUp, w));
		v = glm::normalize(glm::cross(w, u));
	};

	glm::vec3 getRayAt(int x, int y) {
		float j = (float) x + 0.5;
		float i = (float) y + 0.5;
		const float aspect = (float) width / (float) height;
		const float tany = glm::tan(fovDeg * glm::pi<float>() / 360.f); //conv to rad and half fovy
		const float tanx = tany * aspect;
		const float halfW = (float) width / 2.f;
		const float halfH = (float) height / 2.f;

		const float a = tanx * ((j - halfW) / halfW);
		const float b = tany * ((halfH - i) / halfH);
		return glm::normalize(a * u + b * v - w);
	};
};

enum GeometryType {
	TRIANGLE,
	SPHERE,
};

enum LightType {
	POINT,
	DIRECTIONAL,
};

struct Light {
	glm::vec3 position;
	glm::vec3 color;
	LightType type;
	glm::vec3 attenuation;	//c0, c1, c2
};

struct IndexedGeometry {
	GeometryType geometryType;
	glm::vec3 ambientColor = glm::vec3(0, 0, 0);
	glm::vec3 diffuseColor = glm::vec3(0, 0, 0);
	glm::vec3 specularColor = glm::vec3(0, 0, 0);
	glm::vec3 emissionColor = glm::vec3(0, 0, 0);
	float shininess;

	glm::mat4 transform;
	glm::mat4 inverseTransform;
	glm::mat3 inverseTransposeTransform;

	int vertexIndices[3];

	glm::vec3 position;
	float radius;
};

struct SceneReader {
	Camera camera;
	std::vector<Light> lights;
	std::vector<glm::vec3> vertices;
	std::vector<IndexedGeometry> geometry;

	// save all occurring material values, assign index to geometry


	std::string outputFilename = "";

	const float epsilonBias = 0.001f;

	void readScene(std::string filename) {
        glm::vec3 cur_diffuseColor(0, 0, 0);
        glm::vec3 cur_ambientColor(0, 0, 0);
        glm::vec3 cur_specularColor(0, 0, 0);
        glm::vec3 cur_emissionColor(0, 0, 0);
        glm::vec3 cur_attenuationTerms(0, 0, 1);
        float cur_shininessValue = 40;

		std::stack<glm::mat4> transformStack;
		transformStack.push(glm::mat4(1.f));	// last unpoppable entry = identity matrix

		std::ifstream file(filename.c_str());
		if (!file.is_open()) {
			std::cout << "file could not be read: " << filename << std::endl;
		}

		std::cout << "reading in " << filename << ": " << std::endl;

		for(std::string line; getline(file, line);) {
			std::stringstream linestream(line);
			std::string cmd;

			linestream >> cmd;

			if(cmd == "size") {
				linestream >> camera.width >> camera.height;

			}
			else if(cmd == "camera") {
				linestream >> camera.eye[0] >> camera.eye[1] >> camera.eye[2]
						   >> camera.center[0] >> camera.center[1] >> camera.center[2]
                           >> camera.worldUp[0] >> camera.worldUp[1] >> camera.worldUp[2]
						   >> camera.fovDeg;
			}
			else if(cmd == "point" || cmd == "directional") {
				Light light;
				linestream >> light.position[0] >> light.position[1]>> light.position[2]
                           >> light.color[0] >> light.color[1]>> light.color[2];
				if(cmd == "point") {
					light.type = LightType::POINT;
					light.attenuation = cur_attenuationTerms;
				}
				else {
					light.type = LightType::DIRECTIONAL;
				}

				std::cout << "light command\t->" << cmd
						<< " position: " <<  glm::to_string(light.position)
						<< " color " << glm::to_string(light.color) << std::endl;

				lights.push_back(light);
			}
			else if(cmd == "vertex") {
				glm::vec3 vertex;
				linestream >> vertex[0] >> vertex[1] >> vertex[2];


				vertices.push_back(vertex);
			}
			else if(cmd == "tri") {
				IndexedGeometry triangle;
				// negative index allowed: no material entry
				triangle.ambientColor = cur_ambientColor;
				triangle.diffuseColor = cur_diffuseColor;
				triangle.specularColor = cur_specularColor;
				triangle.emissionColor = cur_emissionColor;
				triangle.shininess = cur_shininessValue;
				triangle.geometryType = GeometryType::TRIANGLE;

				linestream >> triangle.vertexIndices[0] >> triangle.vertexIndices[1] >> triangle.vertexIndices[2];

				triangle.transform = glm::mat4(transformStack.top());
				triangle.inverseTransform = glm::mat4(glm::inverse(transformStack.top()));
				triangle.inverseTransposeTransform = glm::mat3(glm::transpose(glm::inverse(transformStack.top())));

				geometry.push_back(triangle);
			}
			else if(cmd == "sphere") {
				IndexedGeometry sphere;
				// negative index allowed: no material entry
				sphere.ambientColor = cur_ambientColor;
				sphere.diffuseColor = cur_diffuseColor;
				sphere.specularColor = cur_specularColor;
				sphere.emissionColor = cur_emissionColor;
				sphere.shininess = cur_shininessValue;
				sphere.geometryType = GeometryType::SPHERE;

				sphere.transform = glm::mat4(transformStack.top());
				sphere.inverseTransform = glm::mat4(glm::inverse(transformStack.top()));
				sphere.inverseTransposeTransform = glm::mat3(glm::transpose(glm::inverse(transformStack.top())));

				linestream >> sphere.position[0] >> sphere.position[1]>> sphere.position[2] >> sphere.radius;

				geometry.push_back(sphere);
			}
			else if(cmd == "ambient") {
				linestream >> cur_ambientColor[0] >> cur_ambientColor[1] >> cur_ambientColor[2];
			}
			else if(cmd == "specular") {
				linestream >> cur_specularColor[0] >> cur_specularColor[1] >> cur_specularColor[2];

			}
			else if(cmd == "diffuse") {
				linestream >> cur_diffuseColor[0] >> cur_diffuseColor[1] >> cur_diffuseColor[2];
			}
			else if(cmd == "emission") {
				linestream >> cur_emissionColor[0] >> cur_emissionColor[1] >> cur_emissionColor[2];
			}
			else if(cmd == "shininess") {
				linestream >> cur_shininessValue;
				std::cout << "shininess: " << cur_shininessValue << std::endl;
			}
			else if(cmd == "attenuation" ) {
				linestream >> cur_attenuationTerms[0] >> cur_attenuationTerms[1] >> cur_attenuationTerms[2];
				std::cout << "attenuation: " << glm::to_string(cur_attenuationTerms)<< std::endl;
			}
			else if(cmd == "pushTransform") {
				transformStack.push(transformStack.top());
			}
			else if(cmd == "popTransform") {
				if(transformStack.size() > 1) {
					transformStack.pop(); // erase top element
				}
			}
			else if(cmd == "translate") {
				glm::vec3 translationVector;
				linestream >> translationVector[0] >> translationVector[1] >> translationVector[2];
				transformStack.top() = glm::translate(transformStack.top(), translationVector);
			}
			else if(cmd == "rotate") {
				glm::vec3 rotationAxis;
				float degrees;
				linestream >> rotationAxis[0] >> rotationAxis[1] >> rotationAxis[2] >> degrees;
				transformStack.top() *= glm::rotate(degrees*glm::pi<float>()/180.f, rotationAxis);
			}
			else if(cmd == "scale") {
				glm::vec3 scaleVector;
				linestream >> scaleVector[0] >> scaleVector[1] >> scaleVector[2];
				transformStack.top() *= glm::scale(scaleVector);
			}
			else if(cmd == "output") {
				linestream >> outputFilename;
			}
//			on (cmd[0] == '#'|| cmd.empty()) do nothing
//			ignore unrecognized commands
		}
	}

	void printGeomTransforms() {
		for(int i = 0;i < geometry.size(); i++) {
			IndexedGeometry geo = geometry[i];

			std::cout << i << ": " << glm::to_string(geo.transform) << std::endl;
		}
	}
};

#endif /* SRC_READSCENE_H_ */
