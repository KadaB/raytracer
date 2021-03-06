//============================================================================
// Name        : edx_hw3_raytr.cpp
// Author      : 
// Version     :
// Copyright   : Your copyright notice
// Description : Hello World in C++, Ansi-style
//============================================================================
#include <iostream>
#include <string>
#include <fstream>
#include <cstdio>
#include <limits>
#include <omp.h>
#include <cstddef>
#include <chrono>

#include "Image3f.h"

#include <glm/glm.hpp>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui.hpp>

#include "readScene.h"

using namespace std;
using namespace glm;

inline glm::vec3 calc_lighting(glm::vec3 rayDir, glm::vec3 shadowray_direction, glm::vec3 fragmentNormal,
		Material *material, glm::vec3 lightColor) {
    // lambert shading
    const float lambertShade = clamp(glm::dot(glm::normalize(shadowray_direction), fragmentNormal));

    glm::vec3 lambert = material->diffuseColor * lightColor * lambertShade;
    // phong shading
    glm::vec3 halfvec = glm::normalize(glm::normalize(shadowray_direction) + glm::normalize(-rayDir));

    const float phongShade = clamp(glm::dot(halfvec, fragmentNormal));

    const float shinePow = glm::pow(std::max(phongShade, 0.f), material->shininess);
    glm::vec3 phong = material->specularColor * lightColor * shinePow;

    return lambert + phong;
}

glm::vec3 shadowRayTest(FragmentInfo fragmentInfo, glm::vec3 rayDir, SceneReader &sr) {
	glm::vec3 shadowColor(0, 0, 0);
	Material *material = fragmentInfo.material;

	for(auto const& light : sr.lights) {
        if(light.type == LightType::POINT) {
            glm::vec3 shadowray_direction = glm::normalize(light.position - fragmentInfo.position);
            glm::vec3 shadowray_origin = fragmentInfo.position + sr.epsilonBias * shadowray_direction;
            float t_toLight = glm::dot(light.position - fragmentInfo.position, shadowray_direction);	// is distance on normalized ray, avoids square root

            FragmentInfo shadowHitInfo = sr.scene_content->intersect(shadowray_origin, shadowray_direction, t_toLight);

            if(!shadowHitInfo.validHit) {
                float attenuation = light.attenuation[0]
                            + light.attenuation[1] * t_toLight
                            + light.attenuation[2] * t_toLight * t_toLight;

                shadowColor += calc_lighting(rayDir, shadowray_direction, fragmentInfo.normal, material, light.color) / attenuation;
            }
        }
        else if(light.type == LightType::DIRECTIONAL) {
            glm::vec3 shadowray_direction = glm::normalize(light.position);
            glm::vec3 shadowray_origin = fragmentInfo.position + sr.epsilonBias * shadowray_direction;
            FragmentInfo shadowHitInfo = sr.scene_content->intersect(shadowray_origin, shadowray_direction);

            if(!shadowHitInfo.validHit) {
                shadowColor += calc_lighting(rayDir, shadowray_direction, fragmentInfo.normal, material, light.color);
            }
        }
	}

	return clampRGB(shadowColor);
}

glm::vec3 trace(glm::vec3 rayOrigin, glm::vec3 rayDir, SceneReader &sr, const float maxDepth = 5) {
	FragmentInfo fragmentInfo = sr.scene_content->intersect(rayOrigin, glm::normalize(rayDir));
	if(fragmentInfo.validHit) {
		glm::vec3 reflectionColor(0, 0, 0);
		if(maxDepth > 0) {
			// calculate reflectionRay, fragment is in world space
			glm::vec3 fragmentNormal = fragmentInfo.normal;
			glm::vec3 viewDir = glm::normalize(-rayDir);

			glm::vec3 reflectedDir = (2 * glm::dot(viewDir, fragmentNormal) *fragmentNormal) - viewDir;

			glm::vec3 reflectedPos = fragmentInfo.position + sr.epsilonBias * reflectedDir;
			reflectionColor = trace(reflectedPos, reflectedDir, sr, maxDepth - 1);
		}

		// shadowray
		glm::vec3 shadowColor = shadowRayTest(fragmentInfo, rayDir, sr);

		return clampRGB( fragmentInfo.material->ambientColor
                       + fragmentInfo.material->emissionColor
                       + shadowColor
                       + fragmentInfo.material->specularColor * reflectionColor);
	}
	else {
		return glm::vec3(0, 0, 0);
	}
}

void raytrace(std::string scenefilename) {
	SceneReader sr;
	sr.readScene(scenefilename, true);
	sr.camera.updateAxes();

	std::cout<<"initialize image buffer space"<<std::endl;
	const int width = sr.camera.width;
	const int height = sr.camera.height; // image dims
	std::cout<<"Image " << width << " " << height <<std::endl;

	Image3f image(width, height);
	std::cout<<"setting background"<<std::endl;

	std::cout<<"start raytrace" << std::endl;

	auto start = std::chrono::high_resolution_clock::now();

	#pragma omp parallel for
	for(int y = 0; y < height; y++) {
		for(int x = 0; x < width; x++) {
			glm::vec3 rayDir = sr.camera.getRayAt(x, y);

            image.setAt(x, y, trace(sr.camera.eye, rayDir, sr, 5));
		}
	}

	auto finish = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed = finish - start;
	std::cout << "finished raytracing after " << elapsed.count() << " seconds" << std::endl;

	int k = image.display(0);

	if(k == 10 || !sr.outputFilename.empty()) {
		std::string filename =
				sr.outputFilename.empty() ? "raytrace.png" :  sr.outputFilename;
		image.save(filename);
	}
}

int main() {
//	raytrace("res/test.test");
//	raytrace("res/scene1.test");			// Triangle
//	raytrace("res/scene2.test");			// W??rfel
//	raytrace("res/scene3.test");			// table

//	raytrace("res/scene4-ambient.test");	// table
//	raytrace("res/scene4-diffuse.test");
//	raytrace("res/scene4-emission.test");
//	raytrace("res/scene4-specular.test");
	// raytrace("res/scene5.test");		// many spheres
	// raytrace("res/scene6.test");		// cornell box
	raytrace("res/scene7.test");		// dragon

	return 0;
}
