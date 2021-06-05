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

#define VERBOSE

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

//		float Px = (2 * ((x + 0.5) / width) - 1) * glm::tan(fovDeg / 2 * M_PI / 180) * aspect;
//		float Py = (1 - 2 * ((y + 0.5) / height)) * glm::tan(fovDeg / 2 * M_PI / 180);
//		rayDir = glm::normalize(Px * u + Py * v - w);
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
};

struct IndexedGeometry {
	int ambientIndex, diffuseIndex, specularIndex;
	GeometryType geometryType;

	glm::mat4 transform;
	glm::mat4 inverseTransform;

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
	std::vector<glm::vec3> ambientColors;
	std::vector<glm::vec3> diffuseColors;
	std::vector<glm::vec3> specularColors;

	std::stack<glm::mat4> transformStack;
	glm::mat4 current_transformation = glm::mat4(1.f); // identity matrix

	void readScene(std::string filename) {
		transformStack.push(glm::mat4(1.f));	// last unpoppable entry = identity matrix

#ifdef VERBOSE
		std::cout << "current transformation: " << glm::to_string(current_transformation) << std::endl;
#endif
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

#ifdef VERBOSE
				std::cout << "size command\t->" << cmd
						<< " w: " << camera.width << " h: " << camera.height << std::endl;
#endif
			}
			else if(cmd == "camera") {
				linestream >> camera.eye[0] >> camera.eye[1] >> camera.eye[2]
						   >> camera.center[0] >> camera.center[1] >> camera.center[2]
                           >> camera.worldUp[0] >> camera.worldUp[1] >> camera.worldUp[2]
						   >> camera.fovDeg;

#ifdef VERBOSE
				std::cout << "camera command\t->" << cmd
						<< " eye: " <<  glm::to_string(camera.eye)
						<< " center: " << glm::to_string(camera.center)
						<< " worldUp " << glm::to_string(camera.worldUp)
						<< " field of view: " << camera.fovDeg << std::endl;
#endif
			}
			else if(cmd == "point" || cmd == "directional") {
				Light light;
				linestream >> light.position[0] >> light.position[1]>> light.position[2]
                           >> light.color[0] >> light.color[1]>> light.color[2];
				if(cmd == "point") {
					light.type = LightType::POINT;
				}
				else {
					light.type = LightType::DIRECTIONAL;
				}

#ifdef VERBOSE
				std::cout << "light command\t->" << cmd
						<< " position: " <<  glm::to_string(light.position)
						<< " color " << glm::to_string(light.color) << std::endl;
#endif

				lights.push_back(light);
			}
			else if(cmd == "vertex") {
				glm::vec3 vertex;
				linestream >> vertex[0] >> vertex[1] >> vertex[2];

#ifdef VERBOSE
				std::cout << "vertex command\t->" << cmd
						<< " at " << vertices.size() <<": " << glm::to_string(vertex) << std::endl;
#endif

				vertices.push_back(vertex);
			}
			else if(cmd == "tri") {
				IndexedGeometry triangle;
				// negative index allowed: no material entry
				triangle.ambientIndex = ambientColors.size() - 1;
				triangle.diffuseIndex = diffuseColors.size() - 1;
				triangle.specularIndex = specularColors.size() - 1;
				triangle.geometryType = GeometryType::TRIANGLE;

				linestream >> triangle.vertexIndices[0] >> triangle.vertexIndices[1] >> triangle.vertexIndices[2];

				triangle.transform = glm::mat4(current_transformation);
				triangle.inverseTransform = glm::mat4(glm::transpose(glm::inverse(current_transformation)));
#ifdef VERBOSE
				std::cout << "triangle command->" << cmd
						<< " at " << geometry.size() <<": " << triangle.vertexIndices[0] << " " << triangle.vertexIndices[1] << " " << triangle.vertexIndices[2] << std::endl;
				std::cout << "\ttransform: " << glm::to_string(triangle.transform) << std::endl;
#endif

				geometry.push_back(triangle);
			}
			else if(cmd == "sphere") {
				IndexedGeometry sphere;
				// negative index allowed: no material entry
				sphere.ambientIndex = ambientColors.size() - 1;
				sphere.diffuseIndex = diffuseColors.size() - 1;
				sphere.specularIndex = specularColors.size() - 1;
				sphere.geometryType = GeometryType::SPHERE;

				sphere.transform = glm::mat4(current_transformation);
				sphere.inverseTransform = glm::mat4(glm::transpose(glm::inverse(current_transformation)));

				linestream >> sphere.position[0] >> sphere.position[1]>> sphere.position[2] >> sphere.radius;

#ifdef VERBOSE
				std::cout << "sphere command->" << cmd
						<< " at " << geometry.size() <<": " << sphere.position[0] << " " << sphere.position[1] << " " << sphere.position[2]
                        << " " << sphere.radius << std::endl;
				std::cout << "\ttransform: " << glm::to_string(sphere.transform) << std::endl;
#endif

				geometry.push_back(sphere);
			}
			else if(cmd == "ambient") {
				glm::vec3 color;
				linestream >> color[0] >> color[1] >> color[2];

#ifdef VERBOSE
				std::cout << "ambient command\t->" << cmd
						<< " at " << ambientColors.size() <<": " << color[0] << " " << color[1] << " " << color[2] << std::endl;
#endif

				ambientColors.push_back(color);
			}
			else if(cmd == "specular") {
				glm::vec3 color;
				linestream >> color[0] >> color[1] >> color[2];

#ifdef VERBOSE
				std::cout << "specular command->" << cmd
						<< " at " << specularColors.size() <<": " << color[0] << " " << color[1] << " " << color[2] << std::endl;
#endif

				specularColors.push_back(color);
			}
			else if(cmd == "diffuse") {
				glm::vec3 color;
				linestream >> color[0] >> color[1] >> color[2];

#ifdef VERBOSE
				std::cout << "diffuse command\t->" << cmd
						<< " at " << diffuseColors.size() <<": " << color[0] << " " << color[1] << " " << color[2] << std::endl;
#endif

				diffuseColors.push_back(color);
			}
			else if(cmd == "pushTransform") {
#ifdef VERBOSE
				std::cout << "pushTransform command: pushing current transformation stack on top: " << transformStack.size()  << std::endl;
#endif
				transformStack.push(current_transformation);
			}
			else if(cmd == "popTransform") {
#ifdef VERBOSE
				std::cout << "popTransform command: pop current top() " << transformStack.size() << " transformation from stack" << std::endl;
#endif
				if(transformStack.size() > 1) {
					transformStack.pop(); // erase top element
					current_transformation = glm::mat4(transformStack.top());	// previous top is now current top
#ifdef VERBOSE
					std::cout << "stack top: " << glm::to_string(transformStack.top()) << std::endl;
#endif
				}
			}
			else if(cmd == "translate") {
				glm::vec3 translationVector;
				linestream >> translationVector[0] >> translationVector[1] >> translationVector[2];
				std::cout << "translate cmd: translation vector " << glm::to_string(translationVector) << std::endl;
				std::cout << "\t before" << glm::to_string(current_transformation) << std::endl;
				current_transformation =  glm::translate(glm::mat4(1.0f), translationVector);
#ifdef VERBOSE
				std::cout << "\t after" << glm::to_string(current_transformation) << std::endl;
#endif
			}
			else if(cmd == "rotate") {
				glm::vec3 rotationAxis;
				float degrees;
				linestream >> rotationAxis[0] >> rotationAxis[1] >> rotationAxis[2] >> degrees;
				std::cout << "rotation cmd: " << glm::to_string(rotationAxis) << " degrees: " << degrees << std::endl;
				std::cout << "\t before" << glm::to_string(current_transformation) << std::endl;
				current_transformation = current_transformation * glm::mat4(glm::rotate(degrees, rotationAxis));
				std::cout << "\t after" << glm::to_string(current_transformation) << std::endl;
#ifdef VERBOSE
#endif
			}
			else if(cmd == "scale") {
				glm::vec3 scaleVector;
				linestream >> scaleVector[0] >> scaleVector[1] >> scaleVector[2];
				std::cout << "scale cmd: scale vector " << glm::to_string(scaleVector) << std::endl;
				std::cout << "\t before" << glm::to_string(current_transformation) << std::endl;
				current_transformation = current_transformation * glm::scale(scaleVector);
				std::cout << "\t after" << glm::to_string(current_transformation) << std::endl;
#ifdef VERBOSE
#endif
			}
//			on (cmd[0] == '#'|| cmd.empty()) do nothing
//			ignore unrecognized commands
		}
	}

	void printOutTransformStack() {
		while(!transformStack.empty()) {
			std::cout << glm::to_string(transformStack.top()) << std::endl;
			transformStack.pop();
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
