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

#include "geometries.h"
#include "grid.h"

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/transform.hpp>
#include <glm/gtx/string_cast.hpp>


class PrimitivesGroup : public IIntersectable {
	std::vector<ITransformedIntersectable*> geometries;		// cells of vectors
public:
	PrimitivesGroup(std::vector<ITransformedIntersectable*> *geometries_ptr) {
		for(auto const& geometry_ptr : *geometries_ptr) {
			this->add(geometry_ptr);
        }
	};
	~PrimitivesGroup() { };

	void add(ITransformedIntersectable* geometry_ptr) {
		this->geometries.push_back(geometry_ptr);
	};

	virtual FragmentInfo intersect(glm::vec3 rayOrigin, glm::vec3 rayDir) {
		// all geometries in cell get brute forced
        HitInfo min_hitInfo;
        ITransformedIntersectable *min_geometry_ptf;
        for(auto const& geometry_ptr : this->geometries)  {
            HitInfo hitInfo = geometry_ptr->intersect(rayOrigin, rayDir);

            // has to be intersection at current cell
            if(hitInfo.validHit && hitInfo.t < min_hitInfo.t) {
            	min_hitInfo = hitInfo;
            	min_geometry_ptf = geometry_ptr;
            }
        }

        if(min_hitInfo.validHit) {
        	return FragmentInfo(true, min_hitInfo.t,
        			rayOrigin + min_hitInfo.t * rayDir,
					normalTransform(min_geometry_ptf->transform, min_hitInfo.normal),
					min_hitInfo.material);
        }
        return FragmentInfo();
	};
};

struct SceneReader {
	Camera camera;
	std::vector<Light> lights;
	std::vector<glm::vec3> vertices;
	std::vector<ITransformedIntersectable*> geometries;

	// this is a member that points to either a PrimitiveGroup that gets brute force intersected 
	// or a Grid structure
	std::unique_ptr<IIntersectable> scene_content;

	std::string outputFilename = "";

	const float epsilonBias = 0.001f;

	~SceneReader()  {
		for(auto const& geometry_prt : geometries) {
			delete geometry_prt;
		}
	};

	void readScene(std::string filename, bool useGrid = false) {
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
				int indexA, indexB, indexC;
				linestream >> indexA >> indexB >> indexC;

				Triangle *triangle = new Triangle(vertices[indexA], vertices[indexB], vertices[indexC],
						Material(cur_ambientColor, cur_diffuseColor, cur_specularColor, cur_emissionColor, cur_shininessValue),
						glm::mat4(transformStack.top()));

				geometries.push_back(triangle);
			}
			else if(cmd == "sphere") {
				glm::vec3 center;
				float radius;
				linestream >> center[0] >> center[1]>> center[2] >> radius;
				Sphere *sphere = new Sphere(center, radius,
						Material(cur_ambientColor, cur_diffuseColor, cur_specularColor, cur_emissionColor, cur_shininessValue),
                        glm::mat4(transformStack.top()));

				geometries.push_back(sphere);
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

		if(useGrid) {
			this->scene_content = std::make_unique<Grid>(&geometries);
		}
		else {
			this->scene_content = std::make_unique<PrimitivesGroup>(&geometries);
		}
	}
};

#endif /* SRC_READSCENE_H_ */
